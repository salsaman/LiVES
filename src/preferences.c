// preferences.c
// LiVES (lives-exe)
// (c) G. Finch 2004 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details
// functions dealing with getting/setting user preferences
// TODO - use atom type system for prefs

#include <dlfcn.h>

#include "main.h"
#include "paramwindow.h"
#include "callbacks.h"
#include "resample.h"
#include "plugins.h"
#include "rte_window.h"
#include "interface.h"
#include "startup.h"
#include "rfx-builder.h"
#include "effects-weed.h"
#include "multitrack-gui.h"
#include "setup.h"

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

static LiVESWidget *saved_closebutton, *saved_applybutton, *saved_revertbutton;
static  boolean mt_needs_idlefunc;

static int nmons;

static uint32_t prefs_current_page;

static void select_pref_list_row(uint32_t selected_idx, _prefsw *prefsw);

static LiVESList *allprefs = NULL;



// new prefs model. All prefs will be stored in WEED plants, with


static weed_plant_t *define_pref(const char *pref_idx, void *pref_ptr, int32_t vtype, void *pdef, uint32_t flags) {
  // TODO...
  /* lives_object_instance_t *pref = lives_pref_inst_create(PLAYER_SUBTYPE_AUDIO); */
  /// -> lives_object_declare_attribute(pref, PREF_ATTR_IDX, WEED_SEED_STRING);
  /// -> lives_object_declare_attribute(pref, PREF_ATTR_VALUE, WEED_SEED_VOIDPTR);
  /* lives_pref_set_idx(pref, pref_idx); */
  /* lives_pref_set_varptr(pref, prefptr); */
  /* lives_pref_set_widget(pref, widget); */
  // OR
  // lives_attribute_set_param_type(pref, PREF_ATTR_VARPTR, label, WEED_PARAM_INTEGER);
  // txfuncs: OBJ_INTENTION_BACKUP, RESTORE, SET_VALUE, GET_VALUE,

  weed_plant_t *prefplant = lives_plant_new(LIVES_PLANT_PREFERENCE);
  weed_set_string_value(prefplant, LIVES_LEAF_PREF_IDX, pref_idx);
  weed_set_voidptr_value(prefplant, LIVES_LEAF_VARPTR, pref_ptr);
  weed_leaf_set(prefplant, WEED_LEAF_DEFAULT, vtype, 1, pdef);
  weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_UNSET);
  weed_set_int_value(prefplant, WEED_LEAF_FLAGS, flags);
  allprefs = lives_list_append(allprefs, prefplant);
  return prefplant;
}


void free_prefs(void) {
  for (LiVESList *list = allprefs; list; list = list->next) {
    weed_plant_t *plant = (weed_plant_t *)list->data;
    weed_plant_free(plant);
    list->data = NULL;
  }
  lives_list_free_all(&allprefs);
}


void init_prefs(void) {
  if (allprefs) return;

  // PRREF_IDX, pref-><...>, default
  DEFINE_PREF_BOOL(POGO_MODE, pogo_mode, FALSE, 0);
  DEFINE_PREF_BOOL(SHOW_TOOLBAR, show_tool, TRUE, 0);

  DEFINE_PREF_DOUBLE(REC_STOP_GB, rec_stop_gb, DEF_REC_STOP_GB, 0);
  DEFINE_PREF_INT(REC_STOP_QUOTA, rec_stop_quota, 90, 0);
  DEFINE_PREF_BOOL(REC_STOP_DWARN, rec_stop_dwarn, TRUE, 0);

  DEFINE_PREF_INT(FOCUS_STEAL, focus_steal, FOCUS_STEAL_DEF, PREF_FLAG_INCOMPLETE)

  DEFINE_PREF_BOOL(PB_HIDE_GUI, pb_hide_gui, FALSE, PREF_FLAG_EXPERIMENTAL);
  DEFINE_PREF_BOOL(SELF_TRANS, tr_self, FALSE, PREF_FLAG_EXPERIMENTAL);
  //DEFINE_PREF_BOOL(GENQ_MODE, genq_mode, FALSE);
  DEFINE_PREF_INT(DLOAD_MATMET, dload_matmet, LIVES_MATCH_CHOICE, 0);
  DEFINE_PREF_INT(WEBCAM_MATMET, webcam_matmet, LIVES_MATCH_AT_MOST, 0);
  DEFINE_PREF_STRING(DEF_AUTHOR, def_author, 1024, "", 0);
}



static weed_plant_t *find_pref(const char *pref_idx) {
  for (LiVESList *list = allprefs; list; list = list->next) {
    weed_plant_t *prefplant = (weed_plant_t *)list->data;
    char *xpref_idx = weed_get_string_value(prefplant, LIVES_LEAF_PREF_IDX, NULL);
    if (!lives_strcmp(xpref_idx, pref_idx)) {
      lives_free(xpref_idx);
      return list->data;
    }
    lives_free(xpref_idx);
  }
  return NULL;
}


static void remove_pref(const char *pref_idx) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  if (prefplant) {
    allprefs = lives_list_remove_data(allprefs, prefplant, FALSE);
    weed_plant_free(prefplant);
  }
}


void load_pref(const char *pref_idx) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  if (!prefplant) return;
  else {
    void *ppref = weed_get_voidptr_value(prefplant, LIVES_LEAF_VARPTR, NULL);
    int32_t type = weed_leaf_seed_type(prefplant, WEED_LEAF_DEFAULT);
    switch (type) {
    case WEED_SEED_BOOLEAN: {
      boolean bdef = weed_get_boolean_value(prefplant, WEED_LEAF_DEFAULT, NULL);
      *(boolean *)ppref = get_boolean_prefd(pref_idx, bdef);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
      break;
    }
    case WEED_SEED_INT: {
      int idef = weed_get_int_value(prefplant, WEED_LEAF_DEFAULT, NULL);
      *(int *)ppref = get_int_prefd(pref_idx, idef);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
      break;
    }
    case WEED_SEED_DOUBLE: {
      double ddef = weed_get_double_value(prefplant, WEED_LEAF_DEFAULT, NULL);
      *(double *)ppref = get_double_prefd(pref_idx, ddef);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
      break;
    }
    case WEED_SEED_STRING: {
      int slen = weed_get_int_value(prefplant, WEED_LEAF_MAXCHARS, NULL);
      char *sdef = weed_get_string_value(prefplant, WEED_LEAF_DEFAULT, NULL);
      get_string_prefd(pref_idx, (char *)ppref, slen, sdef);
      lives_free(sdef);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
      break;
    }
    default: break;
    }
  }
}


void load_prefs(void) {
  if (!allprefs) init_prefs();
  for (LiVESList *list = allprefs; list; list = list->next) {
    weed_plant_t *prefplant = (weed_plant_t *)list->data;
    if (weed_get_int_value(prefplant, LIVES_LEAF_STATUS, NULL) == PREFSTATUS_UNSET) {
      char *xpref_idx = weed_get_string_value(prefplant, LIVES_LEAF_PREF_IDX, NULL);
      load_pref(xpref_idx);
      lives_free(xpref_idx);
    }
  }
}


boolean update_pref(const char *pref_idx, void *newval, boolean permanent) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  if (prefplant) {
    LiVESWidget *widget = (LiVESWidget *)weed_get_voidptr_value(prefplant, LIVES_LEAF_WIDGET, NULL);
    boolean bval;
    int ival;
    double dval;
    if (widget || newval) {
      void *ppref = weed_get_voidptr_value(prefplant, LIVES_LEAF_VARPTR, NULL);
      int32_t type = weed_leaf_seed_type(prefplant, WEED_LEAF_DEFAULT);
      switch (type) {
      case WEED_SEED_BOOLEAN: {
        boolean bpref;
        if (newval) bval = *(boolean *)newval;
        else bval = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget));
        /// any nonstandard updates here
        pref_factory_bool(pref_idx, bval, permanent);
        ///
        bpref = *(boolean *)ppref;
        if (bpref == bval) goto fail;
        *(boolean *)ppref = bval;
        goto bool_success;
      }
      break;
      case WEED_SEED_INT: {
        int ipref;
        if (newval) ival = *(int *)newval;
        else ival = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(widget));
        /// any nonstandard updates here
        pref_factory_int(pref_idx, &ipref, ival, permanent);
        ///
        ipref = *(int *)ppref;
        if (ipref == ival) goto fail;
        *(int *)ppref = ival;
        goto int_success;
      }
      break;
      case WEED_SEED_DOUBLE: {
        double dpref;
        if (newval) dval = *(double *)newval;
        else dval = lives_spin_button_get_value(LIVES_SPIN_BUTTON(widget));
        /// any nonstandard updates here
        pref_factory_double(pref_idx, &dpref, dval, permanent);
        ///
        dpref = *(double *)ppref;
        if (dpref == dval) goto fail;
        *(double *)ppref = dval;
        goto double_success;
      }
      break;
      default: break;
      }
    }

fail:
    if (prefsw) prefsw->ignore_apply = FALSE;
    return FALSE;

bool_success:
    weed_set_boolean_value(prefplant, WEED_LEAF_VALUE, bval);
    if (prefsw) {
      lives_widget_process_updates(prefsw->prefs_dialog);
      prefsw->ignore_apply = FALSE;
    }
    if (permanent) {
      set_boolean_pref(pref_idx, bval);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
    } else weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_TEMP);
    return TRUE;

int_success:
    weed_set_int_value(prefplant, WEED_LEAF_VALUE, ival);
    if (prefsw) {
      lives_widget_process_updates(prefsw->prefs_dialog);
      prefsw->ignore_apply = FALSE;
    }
    if (permanent) {
      set_int_pref(pref_idx, ival);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
    } else weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_TEMP);
    return TRUE;

double_success:
    weed_set_double_value(prefplant, WEED_LEAF_VALUE, dval);
    if (prefsw) {
      lives_widget_process_updates(prefsw->prefs_dialog);
      prefsw->ignore_apply = FALSE;
    }
    if (permanent) {
      set_double_pref(pref_idx, dval);
      weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_PERM);
    } else weed_set_int_value(prefplant, LIVES_LEAF_STATUS, PREFSTATUS_TEMP);
    return TRUE;
  }
  return FALSE;
}


int get_pref_status(const char *pref_idx) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  if (!prefplant) return PREFSTATUS_UNKNOWN;
  return weed_get_int_value(prefplant, LIVES_LEAF_STATUS, NULL);
}



static void update_prefs(void) {
  for (LiVESList *list = allprefs; list; list = list->next) {
    weed_plant_t *plant = (weed_plant_t *)list->data;
    char *pref_idx = weed_get_string_value(plant, LIVES_LEAF_PREF_IDX, NULL);
    update_pref(pref_idx, NULL, TRUE);
    lives_free(pref_idx);
  }
}


boolean update_boolean_pref(const char *pref_idx, boolean val, boolean permanent) {
  return update_pref(pref_idx, (void *)&val, permanent);
}

boolean update_int_pref(const char *pref_idx, int val, boolean permanent) {
  return update_pref(pref_idx, (void *)&val, permanent);
}

boolean update_double_pref(const char *pref_idx, double val, boolean permanent) {
  return update_pref(pref_idx, (void *)&val, permanent);
}

static LiVESWidget *set_pref_widget(const char *pref_idx, LiVESWidget *widget) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  uint32_t flags = 0;
  if (!prefplant) return NULL;
  else {
    if (widget) {
      uint32_t vtype = weed_leaf_seed_type(prefplant, WEED_LEAF_DEFAULT);
      weed_set_voidptr_value(prefplant, LIVES_LEAF_WIDGET, widget);
      switch (vtype) {
      case WEED_SEED_BOOLEAN:
        if (widget) ACTIVE_W(widget, TOGGLED);
        break;
      case WEED_SEED_INT:
      case WEED_SEED_INT64:
      case WEED_SEED_DOUBLE:
        if (widget) ACTIVE_W(widget, VALUE_CHANGED);
        break;
      case WEED_SEED_STRING:
        if (widget) ACTIVE_W(widget, CHANGED);
        break;
      default: break;
      }
    } else if (weed_plant_has_leaf(prefplant, LIVES_LEAF_WIDGET))
      weed_leaf_delete(prefplant, LIVES_LEAF_WIDGET);
  }
  flags = weed_get_int_value(prefplant, WEED_LEAF_FLAGS, NULL);
  if (widget && (flags & PREF_FLAG_EXPERIMENTAL)) {
    if (prefs->show_dev_opts)
      show_warn_image(widget, _("Experimental: altering the value of this preference\n"
                                "may have undesired consquences - do so only with caution !"));
    else
      lives_widget_set_no_show_all(widget, TRUE);
  }
  return widget;
}


void invalidate_pref_widgets(LiVESWidget *top) {
  for (LiVESList *list = allprefs; list; list = list->next) {
    weed_plant_t *prefplant = (weed_plant_t *)list->data;
    LiVESWidget *widget = (LiVESWidget *)weed_get_voidptr_value(prefplant, LIVES_LEAF_WIDGET, NULL);
    if (widget && (widget == top || lives_widget_is_ancestor(widget, top))) {
      char *pref_idx = weed_get_string_value(prefplant, LIVES_LEAF_PREF_IDX, NULL);
      set_pref_widget(pref_idx, NULL);
      lives_free(pref_idx);
    }
  }
}


LiVESWidget *get_pref_widget(const char *pref_idx) {
  weed_plant_t *prefplant = find_pref(pref_idx);
  if (!prefplant) return NULL;
  return weed_get_voidptr_value(prefplant, LIVES_LEAF_WIDGET, NULL);
}


/** @brief callback to set to make a togglebutton or check_menu_item directly control a boolean pref
    widget is either a togge_button (sets temporary) or a check_menuitem (sets permanent)
    pref must have a corresponding entry in pref_factory_bool()

    See also: on_boolean_toggled()
*/
void toggle_sets_pref(LiVESWidget *widget, livespointer prefidx) {
  if (LIVES_IS_TOGGLE_BUTTON(widget))
    pref_factory_bool((const char *)prefidx,
                      lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(widget)), FALSE);
  else if (LIVES_IS_CHECK_MENU_ITEM(widget))
    pref_factory_bool((const char *)prefidx,
                      lives_check_menu_item_get_active(LIVES_CHECK_MENU_ITEM(widget)), TRUE);
}


#ifdef ENABLE_OSC
static void on_osc_enable_toggled(LiVESToggleButton *t1, livespointer t2) {
  if (prefs->osc_udp_started) return;
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, lives_toggle_button_get_active(t1) ||
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(t2)));
}
#endif


static LiVESResponseType get_pref_inner(const char *filename, const char *key, char *val, int maxlen,
                                        LiVESList *cache) {
  char *com;
  memset(val, 0, maxlen);
  if (!filename) {
    if (cache) {
      char *prefval = get_val_from_cached_list(key, maxlen, cache);
      if (prefval) {
        lives_snprintf(val, maxlen, "%s", prefval);
        lives_free(prefval);
        return LIVES_RESPONSE_YES;
      }
      return LIVES_RESPONSE_NO;
    }
    com = lives_strdup_printf("%s get_pref \"%s\" -", prefs->backend_sync, key);
  } else {
    com = lives_strdup_printf("%s get_clip_value \"%s\" - - \"%s\"", prefs->backend_sync, key,
                              filename);
  }

  lives_popen(com, TRUE, val, maxlen);

  lives_free(com);
  return LIVES_RESPONSE_NONE;
}


LIVES_GLOBAL_INLINE LiVESResponseType get_string_pref(const char *key, char *val, int maxlen) {
  /// get from prefs
  return get_pref_inner(NULL, key, val, maxlen, mainw->prefs_cache);
}


LIVES_GLOBAL_INLINE LiVESResponseType get_string_prefd(const char *key, char *val, int maxlen, const char *def) {
  /// get from prefs
  int ret = get_pref_inner(NULL, key, val, maxlen, mainw->prefs_cache);
  if (ret == LIVES_RESPONSE_NO) lives_snprintf(val, maxlen, "%s", def);
  return ret;
}


LIVES_GLOBAL_INLINE LiVESResponseType get_pref_from_file(const char *filename, const char *key, char *val, int maxlen) {
  /// get from non-prefs
  return get_pref_inner(filename, key, val, maxlen, mainw->gen_cache);
}


LiVESResponseType get_utf8_pref(const char *key, char *val, int maxlen) {
  // get a pref in locale encoding, then convert it to utf8
  char *tmp;
  int retval = get_string_pref(key, val, maxlen);
  tmp = F2U8(val);
  lives_snprintf(val, maxlen, "%s", tmp);
  lives_free(tmp);
  return retval;
}


LiVESList *get_list_pref(const char *key) {
  // get a list of values from a preference
  char buf[65536];
  LiVESList *toks;
  size_t ntoks;
  if (get_string_pref(key, buf, 65535) == LIVES_RESPONSE_NO) return NULL;
  toks = get_token_count_split(buf, '\n', &ntoks);
  return toks;
}


LIVES_GLOBAL_INLINE boolean get_boolean_pref(const char *key) {
  char buffer[16];
  get_string_pref(key, buffer, 16);
  if (!strcmp(buffer, "true")) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE boolean get_boolean_prefd(const char *key, boolean defval) {
  char buffer[16];
  get_string_pref(key, buffer, 16);
  if (!(*buffer)) return defval;
  if (!strcmp(buffer, "true")) return TRUE;
  return FALSE;
}


LIVES_GLOBAL_INLINE int get_int_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return 0;
  return atoi(buffer);
}


LIVES_GLOBAL_INLINE int get_int_prefd(const char *key, int defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return atoi(buffer);
}


LIVES_GLOBAL_INLINE int64_t get_int64_prefd(const char *key, int64_t defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return atol(buffer);
}


LIVES_GLOBAL_INLINE double get_double_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return 0.;
  return lives_strtod(buffer);
}


LIVES_GLOBAL_INLINE double get_double_prefd(const char *key, double defval) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return defval;
  return lives_strtod(buffer);
}


LIVES_GLOBAL_INLINE boolean has_pref(const char *key) {
  char buffer[64];
  get_string_pref(key, buffer, 64);
  if (!(*buffer)) return FALSE;
  return TRUE;
}


boolean get_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  /// this is for leading colours from prefs; for loading from themes
  // use get_theme_colour_pref
  char buffer[64];
  char **array;
  int ntoks;

  if (get_string_pref(key, buffer, 64) == LIVES_RESPONSE_NO) return FALSE;
  if (!(*buffer)) return FALSE;
  if ((ntoks = get_token_count(buffer, ' ')) < 3) return FALSE;

  array = lives_strsplit(buffer, " ", 4);
  lcol->red = atoi(array[0]);
  lcol->green = atoi(array[1]);
  lcol->blue = atoi(array[2]);
  if (ntoks == 4) lcol->alpha = atoi(array[3]);
  else lcol->alpha = 65535;
  lives_strfreev(array);

  return TRUE;
}


char *get_meta(const char *key) {
  char buff[1024];
  if (!*mainw->metafile) {
    char *tmp = lives_build_filename(prefs->config_datadir, LIVES_METADATA_FILE, NULL);
    lives_snprintf(mainw->metafile, PATH_MAX, "%s", tmp);
    lives_free(tmp);
  }
  if (!mainw->meta_cache) {
    mainw->meta_cache = cache_file_contents(mainw->metafile);
  }
  get_pref_inner(NULL, key, buff, 1024, mainw->meta_cache);
  return lives_strdup(buff);
}


void set_meta(const char *key, const char *value) {
  if (!*mainw->metafile) {
    char *tmp = lives_build_filename(prefs->config_datadir, LIVES_METADATA_FILE, NULL);
    lives_snprintf(mainw->metafile, PATH_MAX, "%s", tmp);
    lives_free(tmp);
    if (!lives_file_test(mainw->metafile, LIVES_FILE_TEST_EXISTS)) {
      lives_touch(mainw->metafile);
      set_meta("", "");
    }
  }
  set_theme_pref(mainw->metafile, key, value);
}


boolean get_theme_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  /// load from mainw->gen_cache
  char *tmp;
  char **array;
  int ntoks;

  tmp = get_val_from_cached_list(key, 64, mainw->gen_cache);
  if (!tmp) return FALSE;

  if ((ntoks = get_token_count(tmp, ' ')) < 3) {
    lives_free(tmp);
    return FALSE;
  }
  array = lives_strsplit(tmp, " ", 4);
  lcol->red = atoi(array[0]);
  lcol->green = atoi(array[1]);
  lcol->blue = atoi(array[2]);
  if (ntoks == 4) lcol->alpha = atoi(array[3]);
  else lcol->alpha = 65535;
  lives_strfreev(array);
  lives_free(tmp);
  return TRUE;
}


static int run_prefs_command(const char *com) {
  int ret = 0;
  do {
    ret = lives_system(com, TRUE) >> 8;
    if (mainw && mainw->is_ready) {
      if (ret == 4) {
        // error writing to temp config file
        char *newrcfile = ensure_extension(prefs->configfile, LIVES_FILE_EXT_NEW);
        ret = do_write_failed_error_s_with_retry(newrcfile, NULL);
        lives_free(newrcfile);
      } else if (ret == 3) {
        // error writing to config file
        ret = do_write_failed_error_s_with_retry(prefs->configfile, NULL);
      } else if (ret != 0) {
        // error reading from config file
        ret = do_read_failed_error_s_with_retry(prefs->configfile, NULL);
      }
    } else ret = 0;
  } while (ret == LIVES_RESPONSE_RETRY);
  return ret;
}


int delete_pref(const char *key) {
  char *com = lives_strdup_printf("%s delete_pref \"%s\"", prefs->backend_sync, key);
  int ret = run_prefs_command(com);
  lives_free(com);
  if (!ret) remove_pref(key);
  return ret;
}


int set_string_pref(const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_string_pref_priority(const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_pref_priority \"%s\" \"%s\"", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_utf8_pref(const char *key, const char *value) {
  // convert to locale encoding
  char *tmp = U82F(value);
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, tmp);
  int ret = run_prefs_command(com);
  lives_free(com);
  lives_free(tmp);
  return ret;
}


void set_theme_pref(const char *themefile, const char *key, const char *value) {
  char *com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, themefile, key, value);
  int ret = 0;
  do {
    if (lives_system(com, TRUE)) {
      ret = do_write_failed_error_s_with_retry(themefile, NULL);
    }
  } while (ret == LIVES_RESPONSE_RETRY);
  lives_free(com);
}


int set_int_pref(const char *key, int value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" %d", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_int64_pref(const char *key, int64_t value) {
  // not used
  char *com = lives_strdup_printf("%s set_pref \"%s\" %"PRId64, prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_double_pref(const char *key, double value) {
  char *com = lives_strdup_printf("%s set_pref \"%s\" %.3f", prefs->backend_sync, key, value);
  int ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_boolean_pref(const char *key, boolean value) {
  char *com;
  int ret;
  if (value) {
    com = lives_strdup_printf("%s set_pref \"%s\" true", prefs->backend_sync, key);
  } else {
    com = lives_strdup_printf("%s set_pref \"%s\" false", prefs->backend_sync, key);
  }
  ret = run_prefs_command(com);
  lives_free(com);
  return ret;
}


int set_list_pref(const char *key, LiVESList *values) {
  // set pref from a list of values
  LiVESList *xlist = values;
  char *string = NULL, *tmp;
  int ret;

  while (xlist) {
    if (!string) string = lives_strdup((char *)xlist->data);
    else {
      tmp = lives_strdup_printf("%s\n|%s", string, (char *)xlist->data);
      lives_free(string);
      string = tmp;
    }
    xlist = xlist->next;
  }

  if (!string) string = lives_strdup("");

  ret = set_string_pref(key, string);

  lives_free(string);
  return ret;
}


void set_theme_colour_pref(const char *themefile, const char *key, lives_colRGBA64_t *lcol) {
  char *myval = lives_strdup_printf("%d %d %d", lcol->red, lcol->green, lcol->blue);
  char *com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, themefile, key, myval);
  lives_system(com, FALSE);
  lives_free(com);
  lives_free(myval);
}


int set_colour_pref(const char *key, lives_colRGBA64_t *lcol) {
  char *myval = lives_strdup_printf("%d %d %d %d", lcol->red, lcol->green, lcol->blue, lcol->alpha);
  char *com = lives_strdup_printf("%s set_pref \"%s\" \"%s\"", prefs->backend_sync, key, myval);
  int ret = run_prefs_command(com);
  lives_free(com);
  lives_free(myval);
  return ret;
}


void set_palette_prefs(boolean save) {
  lives_colRGBA64_t lcol;

  lcol.red = palette->style;
  lcol.green = lcol.blue = lcol.alpha = 0;

  if (save) {
    if (set_colour_pref(THEME_DETAIL_STYLE, &lcol)) return;

    if (set_string_pref(THEME_DETAIL_SEPWIN_IMAGE, mainw->sepimg_path)) return;
    if (set_string_pref(THEME_DETAIL_FRAMEBLANK_IMAGE, mainw->frameblank_path)) return;
  }

  widget_color_to_lives_rgba(&lcol, &palette->normal_fore);
  if (save)
    if (set_colour_pref(THEME_DETAIL_NORMAL_FORE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->normal_back);
  if (save)
    if (set_colour_pref(THEME_DETAIL_NORMAL_BACK, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars_fore);
  if (save)
    if (set_colour_pref(THEME_DETAIL_ALT_FORE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->menu_and_bars);
  if (save)
    if (set_colour_pref(THEME_DETAIL_ALT_BACK, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->info_text);
  if (save)
    if (set_colour_pref(THEME_DETAIL_INFO_TEXT, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->info_base);
  if (save)
    if (set_colour_pref(THEME_DETAIL_INFO_BASE, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_fg);
  if (save)
    if (set_colour_pref(THEME_DETAIL_MT_TCFG, &lcol)) return;

  widget_color_to_lives_rgba(&lcol, &palette->mt_timecode_bg);
  if (save)
    if (set_colour_pref(THEME_DETAIL_MT_TCBG, &lcol)) return;

  if (save) {
    if (set_colour_pref(THEME_DETAIL_AUDCOL, &palette->audcol)) return;
    if (set_colour_pref(THEME_DETAIL_VIDCOL, &palette->vidcol)) return;
    if (set_colour_pref(THEME_DETAIL_FXCOL, &palette->fxcol)) return;

    if (set_colour_pref(THEME_DETAIL_MT_TLREG, &palette->mt_timeline_reg)) return;
    if (set_colour_pref(THEME_DETAIL_MT_MARK, &palette->mt_mark)) return;
    if (set_colour_pref(THEME_DETAIL_MT_EVBOX, &palette->mt_evbox)) return;

    if (set_colour_pref(THEME_DETAIL_FRAME_SURROUND, &palette->frame_surround)) return;

    if (set_colour_pref(THEME_DETAIL_CE_SEL, &palette->ce_sel)) return;
    if (set_colour_pref(THEME_DETAIL_CE_UNSEL, &palette->ce_unsel)) return;

    if (set_string_pref(THEME_DETAIL_SEPWIN_IMAGE, mainw->sepimg_path)) return;
    if (set_string_pref(THEME_DETAIL_FRAMEBLANK_IMAGE, mainw->frameblank_path)) return;
  }
}

void set_vpp(boolean set_in_prefs) {
  // Video Playback Plugin

  if (*future_prefs->vpp_name) {
    if (!lives_utf8_strcasecmp(future_prefs->vpp_name,
                               mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      if (mainw->vpp) {
        if (mainw->ext_playback) vid_playback_plugin_exit();
        close_vid_playback_plugin(mainw->vpp);
        mainw->vpp = NULL;
        if (set_in_prefs) set_string_pref(PREF_VID_PLAYBACK_PLUGIN, "none");
      }
    } else {
      _vid_playback_plugin *vpp;
      if ((vpp = open_vid_playback_plugin(future_prefs->vpp_name, TRUE))) {
        mainw->vpp = vpp;
        if (set_in_prefs) {
          set_string_pref(PREF_VID_PLAYBACK_PLUGIN, mainw->vpp->soname);
          if (!mainw->ext_playback)
            do_error_dialog(_("\n\nVideo playback plugins are only activated in\n"
                              "full screen, separate window (fs) mode\n"));
        }
      }
    }
    if (set_in_prefs) mainw->write_vpp_file = TRUE;
  }

  if (future_prefs->vpp_argv && mainw->vpp) {
    mainw->vpp->fwidth = future_prefs->vpp_fwidth;
    mainw->vpp->fheight = future_prefs->vpp_fheight;
    mainw->vpp->palette = future_prefs->vpp_palette;
    mainw->vpp->fixed_fpsd = future_prefs->vpp_fixed_fpsd;
    mainw->vpp->fixed_fps_numer = future_prefs->vpp_fixed_fps_numer;
    mainw->vpp->fixed_fps_denom = future_prefs->vpp_fixed_fps_denom;
    if (mainw->vpp->fixed_fpsd > 0.) {
      if (mainw->fixed_fpsd != -1. || !((*mainw->vpp->set_fps)(mainw->vpp->fixed_fpsd))) {
        do_vpp_fps_error();
        mainw->vpp->fixed_fpsd = -1.;
        mainw->vpp->fixed_fps_numer = 0;
      }
    }
    if (!(*mainw->vpp->set_palette)(mainw->vpp->palette)) {
      do_vpp_palette_error();
    }
    mainw->vpp->YUV_clamping = future_prefs->vpp_YUV_clamping;

    if (mainw->vpp->set_yuv_palette_clamping)(*mainw->vpp->set_yuv_palette_clamping)(mainw->vpp->YUV_clamping);

    mainw->vpp->extra_argc = future_prefs->vpp_argc;
    mainw->vpp->extra_argv = future_prefs->vpp_argv;
    if (set_in_prefs) mainw->write_vpp_file = TRUE;
  }

  memset(future_prefs->vpp_name, 0, 64);
  future_prefs->vpp_argv = NULL;
}


static void set_workdir_label_text(LiVESLabel *label, const char *dir) {
  char *free_ds;
  char *tmp, *txt;

  if (!is_writeable_dir(dir)) {
    tmp = (_("(Free space = UNKNOWN)"));
  } else {
    free_ds = lives_format_storage_space_string(get_ds_free(dir));
    tmp = lives_strdup_printf(_("(Free space = %s)"), free_ds);
    lives_free(free_ds);
  }

  txt = lives_strdup_printf(_("The work directory is LiVES working directory where opened clips "
                              "and sets are stored.\n"
                              "It should be in a partition with plenty of free disk space.\n\n%s"),
                            tmp);
  lives_free(tmp);
  lives_layout_label_set_text(label, txt);
  lives_free(txt);
}


boolean pref_factory_string(const char *prefidx, const char *newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;
  if (!lives_strcmp(prefidx, PREF_AUDIO_PLAYER)) {
    const char *audio_player = newval;

    if (!(lives_strcmp(audio_player, AUDIO_PLAYER_NONE)) && prefs->audio_player != AUD_PLAYER_NONE) {
      // switch to none
      switch_aud_to_none(permanent);
#if 0
      if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
        mainw->nullaudio_loop = AUDIO_LOOP_PINGPONG;
      else mainw->nullaudio_loop = AUDIO_LOOP_FORWARD;
#endif
      update_all_host_info(); // let fx plugins know about the change
      goto success1;
    }

#ifdef ENABLE_JACK
    if (!(lives_strcmp(audio_player, AUDIO_PLAYER_JACK)) && prefs->audio_player != AUD_PLAYER_JACK) {
      // switch to jack
      if (!capable->has_jackd) {
        do_error_dialogf(_("\nUnable to switch audio players to %s\n%s must be installed first.\n"
                           "See %s\n"), AUDIO_PLAYER_JACK, EXEC_JACKD, JACK_URL);
        goto fail1;
      }
      if (!switch_aud_to_jack(permanent)) {
        // failed
        if (prefs->jack_opts & JACK_OPTS_START_ASERVER) do_jack_no_startup_warn(FALSE);
        else do_jack_no_connect_warn(FALSE);
        // revert text
        if (prefsw) {
          lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
          lives_widget_process_updates(prefsw->prefs_dialog);
        }
        goto fail1;
      } else {
        // success
        if (mainw->loop_cont) {
          if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
            mainw->jackd->loop = AUDIO_LOOP_PINGPONG;
          else mainw->jackd->loop = AUDIO_LOOP_FORWARD;
        }
        update_all_host_info(); // let fx plugins know about the change
        goto success1;
      }
      goto fail1;
    }
#endif

#ifdef HAVE_PULSE_AUDIO
    if ((!lives_strcmp(audio_player, AUDIO_PLAYER_PULSE)
         || !lives_strcmp(audio_player, AUDIO_PLAYER_PULSE_AUDIO)) &&
        prefs->audio_player != AUD_PLAYER_PULSE) {
      // switch to pulseaudio
      if (!capable->has_pulse_audio) {
        do_error_dialogf(_("\nUnable to switch audio players to %s\n%s must be installed first.\nSee %s\n"),
                         AUDIO_PLAYER_PULSE_AUDIO,
                         AUDIO_PLAYER_PULSE_AUDIO,
                         PULSE_AUDIO_URL);
        // revert text
        if (prefsw) {
          lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
          lives_widget_process_updates(prefsw->prefs_dialog);
        }
        goto fail1;
      } else {
        if (!switch_aud_to_pulse(permanent)) {
          // revert text
          if (prefsw) {
            lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->orig_audp_name);
            lives_widget_process_updates(prefsw->prefs_dialog);
          }
          goto fail1;
        } else {
          // success
          if (mainw->loop_cont) {
            if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
              mainw->pulsed->loop = AUDIO_LOOP_PINGPONG;
            else mainw->pulsed->loop = AUDIO_LOOP_FORWARD;
          }
          update_all_host_info(); // let fx plugins know about the change
          goto success1;
        }
      }
    }
#endif
    goto fail1;
  }

#ifdef HAVE_PULSE_AUDIO
  if (!lives_strcmp(prefidx, PREF_PASTARTOPTS)) {
    if (lives_strncmp(newval, prefs->pa_start_opts, 255)) {
      lives_snprintf(prefs->pa_start_opts, 255, "%s", newval);
      if (permanent) set_string_pref(PREF_PASTARTOPTS, prefs->pa_start_opts);
      goto success1;
    }
    goto fail1;
  }
#endif

#ifdef ENABLE_JACK
  if (!lives_strcmp(prefidx, PREF_JACK_ACONFIG)) {
    if (lives_strncmp(newval, prefs->jack_aserver_cfg, PATH_MAX)) {
      lives_snprintf(prefs->jack_aserver_cfg, PATH_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", newval);
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_tserver_cfg, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", newval);
      }
      if (permanent) {
        set_string_pref(PREF_JACK_ACONFIG, prefs->jack_aserver_cfg);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_ACSERVER)) {
    if (lives_strncmp(newval, prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX)) {
      lives_snprintf(prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_aserver_cname, JACK_PARAM_STRING_MAX, "%s", newval);
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_tserver_cname, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_tserver_cname, PATH_MAX, "%s", newval);
      }
      if (permanent) {
        set_string_pref(PREF_JACK_ACSERVER, prefs->jack_aserver_cname);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_ASSERVER)) {
    if (lives_strncmp(newval, prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX)) {
      lives_snprintf(prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_aserver_sname, JACK_PARAM_STRING_MAX, "%s", newval);
#ifdef ENABLE_JACK_TRANSPORT
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_tserver_sname, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_tserver_sname, PATH_MAX, "%s", newval);
      }
#endif
      if (permanent) {
        set_string_pref(PREF_JACK_ASSERVER, prefs->jack_aserver_sname);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }

#ifdef ENABLE_JACK_TRANSPORT
  if (!lives_strcmp(prefidx, PREF_JACK_TCONFIG)) {
    if (lives_strncmp(newval, prefs->jack_tserver_cfg, PATH_MAX)) {
      lives_snprintf(prefs->jack_tserver_cfg, PATH_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", newval);
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_aserver_cfg, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", newval);
      }
      if (permanent) {
        set_string_pref(PREF_JACK_ACONFIG, prefs->jack_tserver_cfg);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_TCSERVER)) {
    if (lives_strncmp(newval, prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX)) {
      lives_snprintf(prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_tserver_cname, JACK_PARAM_STRING_MAX, "%s", newval);
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_aserver_cname, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_aserver_cname, PATH_MAX, "%s", newval);
      }
      if (permanent) {
        set_string_pref(PREF_JACK_ACSERVER, prefs->jack_tserver_cname);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_TSSERVER)) {
    if (lives_strncmp(newval, prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX)) {
      lives_snprintf(prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX, "%s", newval);
      lives_snprintf(future_prefs->jack_tserver_sname, JACK_PARAM_STRING_MAX, "%s", newval);
      if (prefs->jack_srv_dup) {
        lives_snprintf(prefs->jack_aserver_sname, PATH_MAX, "%s", newval);
        lives_snprintf(future_prefs->jack_aserver_sname, PATH_MAX, "%s", newval);
      }
      if (permanent) {
        set_string_pref(PREF_JACK_TSSERVER, prefs->jack_tserver_sname);
        mainw->prefs_changed |= PREFS_JACK_CHANGED;
      }
      goto success1;
    }
    goto fail1;
  }
#endif
#endif

  if (!lives_strcmp(prefidx, PREF_MIDI_RCV_CHANNEL)) {
    if (strlen(newval) > 2) {
      if (prefs->midi_rcv_channel != -1) {
        prefs->midi_rcv_channel = -1;
        if (permanent) set_int_pref(PREF_MIDI_RCV_CHANNEL, prefs->midi_rcv_channel);
        goto success1;
      }
    } else if (prefs->midi_rcv_channel != atoi(newval)) {
      prefs->midi_rcv_channel = atoi(newval);
      if (permanent) set_int_pref(PREF_MIDI_RCV_CHANNEL, prefs->midi_rcv_channel);
      goto success1;
    }
    goto fail1;
  }

#ifdef ENABLE_JACK
  if (!lives_strcmp(prefidx, PREF_JACK_ADRIVER)) {
    if (newval) set_string_pref(PREF_JACK_ADRIVER, newval);
    else set_string_pref(PREF_JACK_ADRIVER, "");
    return TRUE;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_TDRIVER)) {
    if (newval) set_string_pref(PREF_JACK_TDRIVER, newval);
    else set_string_pref(PREF_JACK_TDRIVER, "");
    return TRUE;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_LAST_ASERVER)) {
    set_string_pref(PREF_JACK_LAST_ASERVER, newval);
    return TRUE;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_LAST_ADRIVER)) {
    set_string_pref(PREF_JACK_LAST_ADRIVER, newval);
    return TRUE;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_LAST_TSERVER)) {
    if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      set_string_pref(PREF_JACK_LAST_TSERVER, newval);
    else
      delete_pref(PREF_JACK_LAST_TSERVER);
    return TRUE;
  }

  if (!lives_strcmp(prefidx, PREF_JACK_LAST_TDRIVER)) {
    if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      set_string_pref(PREF_JACK_LAST_TDRIVER, newval);
    else
      delete_pref(PREF_JACK_LAST_TDRIVER);
    return TRUE;
  }
#endif

  if (!lives_strcmp(prefidx, PREF_CMDLINE_ARGS)) {
    // TODO - parse args format
    if (lives_strncmp(newval, prefs->cmdline_args, 2048)) {
      if (prefs->cmdline_args) lives_free(prefs->cmdline_args);
      prefs->cmdline_args = lives_strndup(newval, 2048);
      capable->extracmds_idx = 0;
      if (permanent) {
        if (*prefs->cmdline_args) {
          FILE *cmdfile = fopen(capable->extracmds_file[0], "w");
          lives_fputs(prefs->cmdline_args, cmdfile);
          fclose(cmdfile);
        } else {
          lives_rm(capable->extracmds_file[0]);
          capable->extracmds_idx = 1;
          char extrabuff[2048];
          size_t buflen;
          if (lives_file_test(capable->extracmds_file[1], LIVES_FILE_TEST_EXISTS)) {
            if ((buflen = lives_fread_string(extrabuff, 2048, capable->extracmds_file[1])) > 0) {
              if (extrabuff[--buflen] == '\n') extrabuff[buflen] = 0;
              prefs->cmdline_args = lives_strdup(extrabuff);
	      // *INDENT-OFF*
	    }}}}
      // *INDENT-ON*
      goto success1;
    }
    goto fail1;
  }

  // unfortunately we cannot automate setting the actual pref, we lack the pref buffer size
  set_string_pref(prefidx, newval);
  return TRUE;

fail1:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success1:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  return TRUE;
}


boolean pref_factory_utf8(const char *prefidx, const char *newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_INTERFACE_FONT)) {
    if (*newval && (!capable->def_fontstring || !*capable->def_fontstring || !permanent ||
                    lives_strcmp(newval, capable->def_fontstring))) {
#if GTK_CHECK_VERSION(3, 16, 0)
      char *tmp;
#endif
      if (newval != capable->def_fontstring) {
        lives_freep((void **)&capable->def_fontstring);
        capable->def_fontstring = lives_strdup(newval);
      }
      lives_freep((void **)&capable->font_name);
      lives_freep((void **)&capable->font_fam);
      lives_freep((void **)&capable->font_stretch);
      lives_freep((void **)&capable->font_style);
      lives_freep((void **)&capable->font_weight);
      lives_parse_font_string(capable->def_fontstring, &capable->font_name, &capable->font_fam, &capable->font_size,
                              &capable->font_stretch, &capable->font_style, &capable->font_weight);
#if GTK_CHECK_VERSION(3, 16, 0)
      tmp = lives_strdup_printf("%dpx", capable->font_size);
      set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "font-size", tmp);
      lives_free(tmp);
      set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "font-family", capable->font_fam);
      set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "font-stretch", capable->font_stretch);
      set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "font-style", capable->font_style);
      set_css_value_direct(NULL, LIVES_WIDGET_STATE_NORMAL, "*", "font-weight", capable->font_weight);
#endif
      goto success;
    }
    goto fail;
  }

  if (!lives_strcmp(prefidx, PREF_OMC_JS_FNAME)) {
    if (lives_strncmp(newval, prefs->omc_js_fname, PATH_MAX)) {
      lives_snprintf(prefs->omc_js_fname, PATH_MAX, "%s", newval);
      goto success;
    }
    goto fail;
  }

  if (!lives_strcmp(prefidx, PREF_OMC_MIDI_FNAME)) {
    if (lives_strncmp(newval, prefs->omc_midi_fname, PATH_MAX)) {
      lives_snprintf(prefs->omc_midi_fname, PATH_MAX, "%s", newval);
      goto success;
    }
    goto fail;
  }

fail:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_utf8_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_bool(const char *prefidx, boolean newval, boolean permanent) {
  // this is called from lbindings.c which in turn is called from liblives.cpp

  // can also be called from other places

  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_SEPWIN)) {
    if (mainw->sep_win == newval) goto fail2;
    on_sepwin_pressed(NULL, NULL);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRCRASH)) {
    if (prefs->rr_crash == newval) goto fail2;
    prefs->rr_crash = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRSUPER)) {
    if (prefs->rr_super == newval) goto fail2;
    prefs->rr_super = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRPRESMOOTH)) {
    if (prefs->rr_pre_smooth == newval) goto fail2;
    prefs->rr_pre_smooth = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRQSMOOTH)) {
    if (prefs->rr_qsmooth == newval) goto fail2;
    prefs->rr_qsmooth = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRAMICRO)) {
    if (prefs->rr_amicro == newval) goto fail2;
    prefs->rr_amicro = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_RRRAMICRO)) {
    if (prefs->rr_ramicro == newval) goto fail2;
    prefs->rr_ramicro = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_QUOTA)) {
    if (prefs->show_disk_quota == newval) goto fail2;
    prefs->show_disk_quota = newval;
    /// allow dialog checkbutton to set permanent pref
    permanent = TRUE;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_MSG_START)) {
    if (prefs->show_msgs_on_startup == newval) goto fail2;
    prefs->show_msgs_on_startup = newval;
    /// allow dialog checkbutton to set permanent pref
    permanent = TRUE;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_AUTOCLEAN_TRASH)) {
    if (prefs->autoclean == newval) goto fail2;
    prefs->autoclean = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_MT_SHOW_CTX)) {
    if (prefs->mt_show_ctx == newval) goto fail2;
    prefs->mt_show_ctx = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_PREF_TRASH)) {
    if (prefs->pref_trash == newval) goto fail2;
    prefs->pref_trash = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_BUTTON_ICONS)) {
    if (prefs->show_button_images == newval) goto fail2;
    prefs->show_button_images = widget_opts.show_button_images = newval;
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    if (prefsw) lives_widget_queue_draw(prefsw->prefs_dialog);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_MENU_ICONS)) {
    if (prefs->show_menu_images == newval) goto fail2;
    prefs->show_menu_images = newval;
    mainw->prefs_changed |= PREFS_THEME_MINOR_CHANGE;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_EXTRA_COLOURS)) {
    if (prefs->extra_colours == newval) goto fail2;
    prefs->extra_colours = newval;
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    if (prefsw) lives_widget_queue_draw(prefsw->prefs_dialog);
    goto success2;
  }

  // show recent
  if (!lives_strcmp(prefidx, PREF_SHOW_RECENT_FILES)) {
    if (prefs->show_recent == newval) goto fail2;
    prefs->show_recent = newval;
    if (newval) {
      lives_widget_set_no_show_all(mainw->recent_menu, FALSE);
      lives_widget_show(mainw->recent_menu);
      if (mainw->multitrack) {
        lives_widget_set_no_show_all(mainw->multitrack->recent_menu, FALSE);
        lives_widget_show(mainw->multitrack->recent_menu);
      }
    } else {
      lives_widget_set_no_show_all(mainw->recent_menu, FALSE);
      lives_widget_hide(mainw->recent_menu);
      if (mainw->multitrack) {
        lives_widget_set_no_show_all(mainw->multitrack->recent_menu, TRUE);
        lives_widget_hide(mainw->multitrack->recent_menu);
      }
    }
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check), newval);
  }

  if (!lives_strcmp(prefidx, PREF_MSG_NOPBDIS)) {
    if (prefs->msgs_nopbdis == newval) goto fail2;
    prefs->msgs_nopbdis = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_nopbdis), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_MSGS)) {
    if (future_prefs->show_msg_area == newval) goto fail2;
    future_prefs->show_msg_area = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis), newval);
    if (!newval || capable->can_show_msg_area) {
      prefs->show_msg_area = newval;
      if (!prefs->show_msg_area) {
        lives_widget_hide(mainw->message_box);
        lives_widget_set_no_show_all(mainw->message_box, TRUE);
        if (mainw->multitrack) {
          lives_widget_hide(mainw->multitrack->message_box);
          lives_widget_set_no_show_all(mainw->multitrack->message_box, TRUE);
        }
      } else {
        lives_widget_set_no_show_all(mainw->message_box, FALSE);
        if (mainw->multitrack) {
          lives_widget_set_no_show_all(mainw->multitrack->message_box, FALSE);
          lives_widget_show(mainw->multitrack->message_box);
          msg_area_scroll_to_end(mainw->multitrack->msg_area, mainw->multitrack->msg_adj);
          lives_widget_queue_draw(mainw->multitrack->msg_area);
        } else {
          lives_widget_show_all(mainw->message_box);
          msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
          lives_widget_queue_draw(mainw->msg_area);
        }
      }
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_USE_SCREEN_GAMMA)) {
    if (prefs->use_screen_gamma == newval) goto fail2;
    prefs->use_screen_gamma = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_screengamma), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_LETTERBOX)) {
    if (prefs->letterbox == newval) goto fail2;
    prefs->letterbox = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lb), newval);
    if (mainw->multitrack) {
      lives_signal_handler_block(mainw->letter, mainw->lb_func);
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->letter), newval);
      lives_signal_handler_unblock(mainw->letter, mainw->lb_func);
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_LETTERBOX_MT)) {
    if (prefs->letterbox_mt == newval) goto fail2;
    prefs->letterbox_mt = newval;
    if (permanent) {
      future_prefs->letterbox_mt = newval;
      if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbmt), newval);
    }
    if (mainw->multitrack && mainw->multitrack->event_list)
      mt_show_current_frame(mainw->multitrack, FALSE);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_NO_LB_GENS)) {
    if (prefs->no_lb_gens == newval) goto fail2;
    prefs->no_lb_gens = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->no_lb_gens), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_LETTERBOX_ENC)) {
    if (prefs->enc_letterbox == newval) goto fail2;
    prefs->enc_letterbox = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbenc), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_PBQ_ADAPTIVE)) {
    if (prefs->pbq_adaptive == newval) goto fail2;
    prefs->pbq_adaptive = newval;
    if (prefsw) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pbq_adaptive), newval);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_VJMODE)) {
    if (future_prefs->vj_mode == newval) goto fail2;
    if (mainw && mainw->vj_mode)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->vj_mode), newval);
    future_prefs->vj_mode = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_DEVOPTS)) {
    if (prefs->show_dev_opts == newval) goto fail2;
    if (mainw && mainw->show_devopts)
      lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->show_devopts), newval);
    prefs->show_dev_opts = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_REC_EXT_AUDIO)) {
    boolean success = FALSE;
    boolean rec_ext_audio = newval;
    if (rec_ext_audio && prefs->audio_src == AUDIO_SRC_INT) {
      prefs->audio_src = AUDIO_SRC_EXT;
      if (permanent) {
        set_int_pref(PREF_AUDIO_SRC, AUDIO_SRC_EXT);
        future_prefs->audio_src = prefs->audio_src;
      }
      success = TRUE;
    } else if (!rec_ext_audio && prefs->audio_src == AUDIO_SRC_EXT) {
      prefs->audio_src = AUDIO_SRC_INT;
      if (permanent) {
        set_int_pref(PREF_AUDIO_SRC, AUDIO_SRC_INT);
        future_prefs->audio_src = prefs->audio_src;
      }
      success = TRUE;
    }
    if (success) {
      if (prefsw && permanent) {
        if (prefs->audio_src == AUDIO_SRC_EXT)
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), TRUE);
        else
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rintaudio), TRUE);
      }
      lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_checkbutton),
                                          prefs->audio_src == AUDIO_SRC_EXT);

      lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->int_audio_checkbutton),
                                          prefs->audio_src == AUDIO_SRC_INT);
      goto success2;
    }
    goto fail2;
  }

  if (!lives_strcmp(prefidx, PREF_MT_EXIT_RENDER)) {
    if (prefs->mt_exit_render == newval) goto fail2;
    prefs->mt_exit_render = newval;
    if (prefsw)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render), prefs->mt_exit_render);
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_PUSH_AUDIO_TO_GENS)) {
    if (prefs->push_audio_to_gens == newval) goto fail2;
    prefs->push_audio_to_gens = newval;
    if (prefsw)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pa_gens), prefs->push_audio_to_gens);
    goto success2;
  }

#ifdef HAVE_PULSE_AUDIO
  if (!lives_strcmp(prefidx, PREF_PARESTART)) {
    if (prefs->pa_restart == newval) goto fail2;
    prefs->pa_restart = newval;
    if (prefsw)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart), prefs->pa_restart);
    goto success2;
  }
#endif

  if (!lives_strcmp(prefidx, PREF_SHOW_ASRC)) {
    if (prefs->show_asrc == newval) goto fail2;
    prefs->show_asrc = newval;
    if (prefsw)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_asrc), prefs->show_asrc);
    if (!newval) {
      lives_widget_hide(mainw->int_audio_checkbutton);
      lives_widget_hide(mainw->ext_audio_checkbutton);
      lives_widget_hide(mainw->l1_tb);
      lives_widget_hide(mainw->l2_tb);
      lives_widget_hide(mainw->l3_tb);
    } else {
      lives_widget_show(mainw->int_audio_checkbutton);
      lives_widget_show(mainw->ext_audio_checkbutton);
      lives_widget_show(mainw->l1_tb);
      lives_widget_show(mainw->l2_tb);
      lives_widget_show(mainw->l3_tb);
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_SHOW_TOOLTIPS)) {
    if (prefs->show_tooltips == newval) goto fail2;
    else {
      if (newval) prefs->show_tooltips = newval;
      if (prefsw) {
        if (prefsw->checkbutton_show_ttips)
          lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_ttips), newval);
        set_tooltips_state(prefsw->prefs_dialog, newval);
      }
      set_tooltips_state(mainw->top_vbox, newval);
      if (mainw->multitrack) set_tooltips_state(mainw->multitrack->top_vbox, newval);
      if (fx_dialog[0] && LIVES_IS_WIDGET(fx_dialog[0]->dialog)) set_tooltips_state(fx_dialog[0]->dialog, newval);
      if (fx_dialog[1] && LIVES_IS_WIDGET(fx_dialog[1]->dialog)) set_tooltips_state(fx_dialog[1]->dialog, newval);
      if (rte_window) set_tooltips_state(rte_window, newval);
    }
    // turn off after, or we cannot nullify the ttips
    if (!newval) prefs->show_tooltips = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_HFBWNP)) {
    if (prefs->hfbwnp == newval) goto fail2;
    prefs->hfbwnp = newval;
    if (prefsw)
      lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_hfbwnp), prefs->hfbwnp);
    if (newval) {
      if (!LIVES_IS_PLAYING) {
        lives_widget_hide(mainw->framebar);
      }
    } else {
      if (!LIVES_IS_PLAYING || (LIVES_IS_PLAYING && !prefs->hide_framebar &&
                                (!mainw->fs || (mainw->ext_playback && mainw->vpp &&
                                    !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
                                    !(mainw->vpp->capabilities & VPP_CAN_RESIZE))))) {
        lives_widget_show(mainw->framebar);
      }
    }
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_AR_CLIPSET)) {
    if (prefs->ar_clipset == newval) goto fail2;
    prefs->ar_clipset = newval;
    goto success2;
  }

  if (!lives_strcmp(prefidx, PREF_AR_LAYOUT)) {
    if (prefs->ar_layout == newval) goto fail2;
    prefs->ar_layout = newval;
    goto success2;
  }

fail2:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success2:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_boolean_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_color_button(lives_colRGBA64_t *pcol, LiVESColorButton * cbutton) {
  LiVESWidgetColor col;
  lives_colRGBA64_t lcol;

  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_rgba_equal(widget_color_to_lives_rgba(&lcol, lives_color_button_get_color(cbutton, &col)), pcol)) {
    lives_rgba_copy(pcol, &lcol);
    if (prefsw) {
      lives_widget_process_updates(prefsw->prefs_dialog);
      prefsw->ignore_apply = FALSE;
    }
    return TRUE;
  }

  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;
}


boolean pref_factory_int(const char *prefidx, int *pref, int newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

#ifdef ENABLE_JACK
  if (!lives_strcmp(prefidx, PREF_JACK_OPTS)) {
    newval |= (future_prefs->jack_opts & (JACK_INFO_TEMP_OPTS | JACK_INFO_TEMP_NAMES));
    if (prefs->jack_opts != newval) {
      int diff = newval ^ prefs->jack_opts;
      newval &= ~JACK_INFO_TEMP_OPTS;
      set_int_pref(PREF_JACK_OPTS, newval & ~JACK_INFO_TEMP_NAMES);
      future_prefs->jack_opts = prefs->jack_opts = newval;
      if (prefsw) {
        if (diff & (JACK_OPTS_START_ASERVER | JACK_OPTS_NO_READ_AUTOCON
                    | JACK_OPTS_ENABLE_TCLIENT | JACK_OPTS_PERM_ASERVER
                    | JACK_OPTS_SETENV_ASERVER))
          mainw->prefs_changed |= PREFS_JACK_CHANGED;
        else {
          if (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
            if (diff & (JACK_OPTS_START_TSERVER | JACK_OPTS_PERM_TSERVER | JACK_OPTS_SETENV_TSERVER))
              mainw->prefs_changed |= PREFS_JACK_CHANGED;
	    // *INDENT-OFF*
	  }}}
      // *INDENT-ON*
#ifdef ENABLE_JACK_TRANSPORT
      if (!(prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
          || !(prefs->jack_opts & JACK_OPTS_STRICT_SLAVE))
        jack_transport_make_strict_slave(mainw->jackd_trans, FALSE);
      else
        jack_transport_make_strict_slave(mainw->jackd_trans, TRUE);
#endif
      goto success3;
    }
    goto fail3;
  }
#endif

  if (pref && newval == *pref) goto fail3;

  if (!lives_strcmp(prefidx, PREF_MT_AUTO_BACK)) {
    if (mainw->multitrack) {
      if (newval <= 0 && *pref > 0) {
        if (mainw->multitrack->idlefunc > 0) {
          lives_source_remove(mainw->multitrack->idlefunc);
          mainw->multitrack->idlefunc = 0;
        }
        if (newval == 0) {
          *pref = newval;
          mt_auto_backup(mainw->multitrack);
        }
      }
      if (newval > 0 && *pref <= 0 && mainw->multitrack->idlefunc > 0) {
        mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
      }
    }
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_RTE_KEYS_VIRTUAL)) {
    // if we are showing the rte window, we must destroy and recreate it
    prefs->rte_keys_virtual = newval;
    mainw->prefs_changed |= PREFS_RTE_KEYMODES_CHANGED;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_RTE_MODES_PERKEY)) {
    // if we are showing the rte window, we must destroy and recreate it
    prefs->rte_modes_per_key = newval;
    mainw->prefs_changed |= PREFS_RTE_KEYMODES_CHANGED;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_MAX_MSGS)) {
    if (newval < mainw->n_messages && newval >= 0) {
      free_n_msgs(mainw->n_messages - newval);
      if (prefs->show_msg_area)
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
    }
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_SEPWIN_TYPE)) {
    if (newval == SEPWIN_TYPE_STICKY) {
      if (mainw->sep_win) {
        if (!LIVES_IS_PLAYING) {
          make_play_window();
        }
      } else {
        if (mainw->play_window) {
          play_window_set_title();
        }
      }
    } else {
      if (mainw->sep_win) {
        if (!LIVES_IS_PLAYING) {
          kill_play_window();
        } else {
          play_window_set_title();
        }
      }
    }

    if (permanent) future_prefs->sepwin_type = newval;
    goto success3;
  }

  if (!lives_strcmp(prefidx, PREF_BADFILE_INTENT)) {
    permanent = FALSE;
    goto success3;
  }

  goto success3;

fail3:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success3:
  if (pref) *pref = newval;
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_int_pref(prefidx, newval);
  return TRUE;
}

boolean pref_factory_double(const char *prefidx, double * pref, double newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (pref && newval == *pref) goto fail;

  if (pref) *pref = newval;
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_double_pref(prefidx, newval);
  return TRUE;

fail:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;
}

boolean pref_factory_string_choice(const char *prefidx, LiVESList * list, const char *strval, boolean permanent) {
  int idx = lives_list_strcmp_index(list, (livesconstpointer)strval, TRUE);
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_MSG_TEXTSIZE)) {
    if (idx == prefs->msg_textsize) goto fail4;
    prefs->msg_textsize = idx;
    if (permanent) future_prefs->msg_textsize = prefs->msg_textsize;
    if (prefs->show_msg_area) {
      if (mainw->multitrack)
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->multitrack->msg_adj), mainw->multitrack->msg_area);
      else
        msg_area_scroll(LIVES_ADJUSTMENT(mainw->msg_adj), mainw->msg_area);
    }
    goto success4;
  }

fail4:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success4:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_int_pref(prefidx, idx);
  return TRUE;
}


boolean pref_factory_float(const char *prefidx, float newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_MASTER_VOLUME)) {
    char *ttip;
    if ((LIVES_IS_PLAYING && future_prefs->volume == newval)
        || (!LIVES_IS_PLAYING && prefs->volume == (double)newval)) goto fail5;
    future_prefs->volume = newval;
    ttip = lives_strdup_printf(_("Audio volume (%.2f)"), newval);
    lives_widget_set_tooltip_text(mainw->vol_toolitem, _(ttip));
    lives_free(ttip);
    if (!LIVES_IS_PLAYING) {
      prefs->volume = newval;
    } else permanent = FALSE;
    if (LIVES_IS_RANGE(mainw->volume_scale)) {
      lives_range_set_value(LIVES_RANGE(mainw->volume_scale), newval);
    } else {
      lives_scale_button_set_value(LIVES_SCALE_BUTTON(mainw->volume_scale), newval);
    }
    goto success5;
  }

  if (!lives_strcmp(prefidx, PREF_AHOLD_THRESHOLD)) {
    if (prefs->ahold_threshold == newval) goto fail5;
    prefs->ahold_threshold = newval;
    goto success5;
  }

  if (!lives_strcmp(prefidx, PREF_SCREEN_GAMMA)) {
    if (prefs->screen_gamma == newval) goto fail5;
    prefs->screen_gamma = newval;
    goto success5;
  }

fail5:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success5:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_double_pref(prefidx, newval);
  return TRUE;
}


boolean pref_factory_bitmapped(const char *prefidx, uint32_t bitfield, boolean newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_AUDIO_OPTS)) {
    if (newval && !(prefs->audio_opts & bitfield)) prefs->audio_opts |= bitfield;
    else if (!newval && (prefs->audio_opts & bitfield)) prefs->audio_opts &= ~bitfield;
    else goto fail6;

    if (permanent) set_int_pref(PREF_AUDIO_OPTS, prefs->audio_opts);

    if (prefsw) {
      if (bitfield == AUDIO_OPTS_FOLLOW_FPS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow),
                                       (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_FOLLOW_CLIPS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips),
                                       (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_NO_RESYNC_FPS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->resync_fps),
                                       (prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_FPS) ? FALSE : TRUE);
      }
      if (bitfield == AUDIO_OPTS_NO_RESYNC_VPOS) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->resync_vpos),
                                       (prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS) ? FALSE : TRUE);
      }
      if (bitfield == AUDIO_OPTS_RESYNC_ADIR) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->resync_adir),
                                       (prefs->audio_opts & AUDIO_OPTS_RESYNC_ADIR) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_RESYNC_ACLIP) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->resync_aclip),
                                       (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_LOCKED_FREEZE) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_lock),
                                       (prefs->audio_opts & AUDIO_OPTS_LOCKED_FREEZE) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_LOCKED_PING_PONG) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_ping),
                                       (prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_UNLOCK_RESYNC) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_sync),
                                       (prefs->audio_opts & AUDIO_OPTS_UNLOCK_RESYNC) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_LOCKED_RESET) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->alock_reset),
                                       (prefs->audio_opts & AUDIO_OPTS_LOCKED_RESET) ? TRUE : FALSE);
      }
      if (bitfield == AUDIO_OPTS_AUTO_UNLOCK) {
        lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->auto_unlock),
                                       (prefs->audio_opts & AUDIO_OPTS_AUTO_UNLOCK) ? TRUE : FALSE);
      }
    }
    if (bitfield == AUDIO_OPTS_IS_LOCKED) {
      lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->lock_audio_checkbutton),
                                          (prefs->audio_opts & AUDIO_OPTS_IS_LOCKED) ? TRUE : FALSE);
    }
    goto success6;
  }

  if (!lives_strcmp(prefidx, PREF_OMC_DEV_OPTS)) {
    if (newval && !(prefs->omc_dev_opts & bitfield)) prefs->audio_opts |= bitfield;
    else if (!newval && (prefs->omc_dev_opts & bitfield)) prefs->audio_opts &= ~bitfield;
    else goto fail6;

    if (permanent) set_int_pref(PREF_OMC_DEV_OPTS, prefs->omc_dev_opts);

    if (bitfield == OMC_DEV_JS) {
      if (newval) js_open();
      else js_close();
    } else if (bitfield == OMC_DEV_MIDI) {
      if (!newval) midi_close();
    }
#ifdef ALSA_MIDI
    else if (bitfield == OMC_DEV_FORCE_RAW_MIDI) {
      prefs->use_alsa_midi = !newval;
    } else if (bitfield == OMC_DEV_MIDI_DUMMY) {
      prefs->alsa_midi_dummy = newval;
    }
#endif
    goto success6;
  }

fail6:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success6:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  return TRUE;
}


boolean pref_factory_int64(const char *prefidx, int64_t newval, boolean permanent) {
  if (prefsw) prefsw->ignore_apply = TRUE;

  if (!lives_strcmp(prefidx, PREF_DISK_QUOTA)) {
    if (newval != prefs->disk_quota) {
      future_prefs->disk_quota = prefs->disk_quota = newval;
      goto success7;
    }
    goto fail7;
  }

fail7:
  if (prefsw) prefsw->ignore_apply = FALSE;
  return FALSE;

success7:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  if (permanent) set_int64_pref(prefidx, newval);
  return TRUE;
}


LiVESList *pref_factory_list(const char *prefidx, LiVESList * list) {
  // olist is what we return, varlist is what we set in settings
  LiVESList *olist = list, *varlist = NULL;

  if (prefsw) prefsw->ignore_apply = TRUE;
  if (!list) {
    delete_pref(prefidx);
    goto success;
  }

  if (!lives_strcmp(prefidx, PREF_DISABLED_DECODERS)) {
    // step through prefs->disabled_decoders
    char *tmp;
    for (; list; list = list->next) {
      const lives_decoder_sys_t *dpsys = (const lives_decoder_sys_t *)list->data;
      if (!dpsys || !dpsys->id) continue;
      tmp = lives_strdup_printf("0X%016lX", dpsys->id->uid);
      varlist = lives_list_append(varlist, tmp);
    }
    goto success2;
  }

success2:
  if (varlist) {
    set_list_pref(prefidx, varlist);
    lives_list_free_all(&varlist);
  }
success:
  if (prefsw) {
    lives_widget_process_updates(prefsw->prefs_dialog);
    prefsw->ignore_apply = FALSE;
  }
  return olist;
}


boolean apply_prefs(boolean skip_warn) {
  // set current prefs from prefs dialog
  char prefworkdir[PATH_MAX]; /// locale encoding

  const char *video_open_command = lives_entry_get_text(LIVES_ENTRY(prefsw->video_open_entry));
  const char *cmdline_args = lives_entry_get_text(LIVES_ENTRY(prefsw->cmdline_entry));
  const char *def_vid_load_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->vid_load_dir_entry));
  const char *def_vid_save_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->vid_save_dir_entry));
  const char *def_audio_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->audio_dir_entry));
  const char *def_image_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->image_dir_entry));
  const char *def_proj_dir = lives_entry_get_text(LIVES_ENTRY(prefsw->proj_dir_entry));
  const char *wp_path = lives_entry_get_text(LIVES_ENTRY(prefsw->wpp_entry));
#ifdef HAVE_FREI0R
  const char *frei0r_path = lives_entry_get_text(LIVES_ENTRY(prefsw->frei0r_entry));
#endif
#ifdef HAVE_LIBVISUAL
  const char *libvis_path = lives_entry_get_text(LIVES_ENTRY(prefsw->libvis_entry));
#endif
#ifdef HAVE_LADSPA
  const char *ladspa_path = lives_entry_get_text(LIVES_ENTRY(prefsw->ladspa_entry));
#endif
  const char *sepimg_path = lives_entry_get_text(LIVES_ENTRY(prefsw->sepimg_entry));
  const char *frameblank_path = lives_entry_get_text(LIVES_ENTRY(prefsw->frameblank_entry));

  char workdir[PATH_MAX]; /// locale encoding
  const char *theme = lives_combo_get_active_text(LIVES_COMBO(prefsw->theme_combo));
  const char *audp = lives_combo_get_active_text(LIVES_COMBO(prefsw->audp_combo));
  const char *audio_codec = NULL;
  const char *pb_quality = lives_combo_get_active_text(LIVES_COMBO(prefsw->pbq_combo));

  LiVESWidgetColor colf, colb, colf2, colb2, coli, colt, coltcfg, coltcbg;

  boolean pbq_adap = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pbq_adaptive));
  int pbq = PB_QUALITY_MED;
  int idx;

  boolean needs_restart = FALSE;

  double default_fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_def_fps));
  double ext_aud_thresh = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_ext_aud_thresh)) / 100.;
  boolean load_rfx = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_load_rfx));
  boolean apply_gamma = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_apply_gamma));
  boolean antialias = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_antialias));
  boolean fx_threads = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads));

  boolean lbox = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lb));
  boolean lboxmt = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbmt));
  boolean lboxenc = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_lbenc));
  boolean no_lb_gens = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->no_lb_gens));
  boolean scgamma = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_screengamma));
  double gamma = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gamma));

  int nfx_threads = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_nfx_threads));

  boolean stop_screensaver = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->stop_screensaver_check));
  boolean open_maximised = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->open_maximised_check));
  boolean fs_maximised = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->fs_max_check));
  boolean show_recent = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->recent_check));
  boolean stream_audio_out = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio));
  boolean rec_after_pb = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb));

#if GTK_CHECK_VERSION(3, 2, 0)
  char *fontname = lives_font_chooser_get_font(LIVES_FONT_CHOOSER(prefsw->fontbutton));
#endif

  int max_msgs = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->nmessages_spin));
  const char *msgtextsize = lives_combo_get_active_text(LIVES_COMBO(prefsw->msg_textsize_combo));
  boolean msgs_unlimited = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_unlimited));
  boolean msgs_nopbdis = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_nopbdis));
  boolean msgs_mbdis = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis));

  uint64_t ds_warn_level = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds)) * 1000000;
  uint64_t ds_crit_level = (uint64_t)lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_crit_ds)) * 1000000;

#define WARN_BIT(bname) boolean warn_##bname = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON\
									      (prefsw->checkbutton_warn_##bname))

  WARN_BIT(fps); WARN_BIT(save_set); WARN_BIT(mplayer); WARN_BIT(missplugs); WARN_BIT(prefix); WARN_BIT(dup_set);
  WARN_BIT(layout_clips); WARN_BIT(layout_close); WARN_BIT(fsize);
  WARN_BIT(layout_delete); WARN_BIT(layout_shift); WARN_BIT(layout_alter); WARN_BIT(discard_layout); WARN_BIT(mt_no_jack);
  WARN_BIT(layout_adel); WARN_BIT(layout_ashift); WARN_BIT(layout_aalt); WARN_BIT(layout_wipe); WARN_BIT(mt_achans);
  WARN_BIT(layout_gamma); WARN_BIT(layout_popup); WARN_BIT(layout_reload); WARN_BIT(dis_dec); WARN_BIT(dmgd_audio);
  WARN_BIT(mt_backup_space); WARN_BIT(after_crash); WARN_BIT(no_pulse); WARN_BIT(layout_lb); WARN_BIT(vjmode_enter);

  boolean warn_after_dvgrab =
#ifdef HAVE_LDVGRAB
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_after_dvgrab));
#else
    !(prefs->warning_mask & WARN_MASK_AFTER_DVGRAB);
#endif
  boolean warn_yuv4m_open =
#ifdef HAVE_YUV4MPEG
    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_warn_yuv4m_open));
#else
    !(prefs->warning_mask & WARN_MASK_OPEN_YUV4M);
#endif

#ifdef ENABLE_JACK
  WARN_BIT(jack_scrpts);
#endif

  boolean midisynch = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->check_midi));
  boolean instant_open = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open));
  boolean auto_deint = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_deint));
  boolean auto_trim = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_auto_trim));
  boolean concat_images = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_concat_images));
  boolean ins_speed = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ins_speed));
  boolean show_player_stats = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_stats));
  boolean ext_jpeg = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jpeg));
  boolean mouse_scroll = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mouse_scroll));
  boolean ce_maxspect = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_ce_maxspect));
  boolean show_button_icons = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_button_icons));
  boolean show_menu_icons = FALSE;
  boolean extra_colours = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_extra_colours));
  boolean show_asrc = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_show_asrc));
  boolean show_ttips = prefsw->checkbutton_show_ttips == NULL ? FALSE : lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(
                         prefsw->checkbutton_show_ttips));
  boolean hfbwnp = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_hfbwnp));

  int fsize_to_warn = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_fsize));
  int dl_bwidth = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_bwidth));
  int ocp = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_ocp));

  boolean rec_frames = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rframes));
  boolean rec_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rfps));
  boolean rec_effects = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->reffects));
  boolean rec_clips = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rclips));
  boolean rec_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->raudio));
  boolean rec_audio_alock = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->raudio_alock));
  boolean pa_gens = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pa_gens));
  boolean rec_ext_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio));
#ifdef RT_AUDIO
  boolean rec_desk_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rdesk_audio));
#endif

  boolean mt_enter_prompt = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_enter_prompt));
  boolean render_prompt = !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_render_prompt));

  int mt_def_width = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_width));
  int mt_def_height = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_height));
  int mt_def_fps = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_def_fps));
  int mt_def_arate = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
  int mt_def_achans = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
  int mt_def_asamps = atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
  int mt_def_signed_endian = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned)) *
                             AFORM_UNSIGNED + lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))
                             * AFORM_BIG_ENDIAN;
  int mt_undo_buf = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_undo_buf));

  boolean mt_exit_render = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_mt_exit_render));
  boolean mt_enable_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton));
  boolean mt_pertrack_audio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton));
  int mt_backaudio = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton)) ? 1 : 0;

  boolean mt_autoback_always = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always));
  boolean mt_autoback_never = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never));

  int mt_autoback_time = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time));
  int max_disp_vtracks = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_max_disp_vtracks));
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));

  boolean ce_thumbs = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->ce_thumbs));

  boolean forcesmon = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon));
  boolean startup_ce = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce));

  boolean show_msgstart = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->cb_show_msgstart));
  boolean show_quota = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->cb_show_quota));
  boolean autoclean = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->cb_autoclean));

#ifdef ENABLE_JACK
#ifdef ENABLE_JACK_TRANSPORT
  boolean jack_master = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master));
  boolean jack_client = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client));
  boolean jack_tb_start = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start));
  boolean jack_mtb_start = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_start));
  boolean jack_mtb_update = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_update));
  boolean jack_tb_client = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client));
#endif

  boolean jack_stricts = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_stricts));
  boolean jack_read_autocon = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_read_autocon));

  boolean jack_tstart = !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jack_tcerror));
  boolean jack_tperm = prefs->jack_opts & JACK_OPTS_PERM_TSERVER;

  boolean jack_astart = !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jack_acerror));
  boolean jack_aperm = prefs->jack_opts & JACK_OPTS_PERM_ASERVER;

  boolean jack_trans = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jack_trans));

  const char *jack_acname = lives_entry_get_text(LIVES_ENTRY(prefsw->jack_acname));
  const char *jack_tcname = lives_entry_get_text(LIVES_ENTRY(prefsw->jack_tcname));

  boolean jack_acdef = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jack_acdef));
  boolean jack_tcdef = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->jack_tcdef));

  uint32_t jack_opts =
    JACK_OPTS_TRANSPORT_SLAVE * jack_client + JACK_OPTS_TRANSPORT_MASTER * jack_master +
    JACK_OPTS_START_TSERVER * jack_tstart + JACK_OPTS_START_ASERVER
    * jack_astart + JACK_OPTS_PERM_ASERVER * jack_aperm + JACK_OPTS_PERM_TSERVER * jack_tperm
    + JACK_OPTS_ENABLE_TCLIENT * jack_trans
    + JACK_OPTS_STRICT_SLAVE * jack_stricts + JACK_OPTS_TIMEBASE_START * jack_tb_start +
    JACK_OPTS_TIMEBASE_LSTART * jack_mtb_start + JACK_OPTS_TIMEBASE_SLAVE * jack_tb_client
    + JACK_OPTS_TIMEBASE_MASTER * jack_mtb_update
    + JACK_OPTS_NO_READ_AUTOCON * !jack_read_autocon;
#endif

#ifdef RT_AUDIO
  boolean audio_follow_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow));
  boolean audio_follow_clips = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_aclips));
  boolean resync_fps = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->resync_fps));
  boolean resync_vpos = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->resync_vpos));
  boolean resync_adir = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->resync_adir));
  boolean resync_aclip = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->resync_aclip));
  boolean freeze_lock = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_lock));
  boolean freeze_ping = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_ping));
  boolean freeze_resync = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->afreeze_sync));
  boolean alock_reset = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alock_reset));
  boolean auto_unlock = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->auto_unlock));
  uint32_t audio_opts = AUDIO_OPTS_FOLLOW_FPS * audio_follow_fps
                        + AUDIO_OPTS_FOLLOW_CLIPS * audio_follow_clips
                        + AUDIO_OPTS_NO_RESYNC_FPS * !resync_fps + AUDIO_OPTS_NO_RESYNC_VPOS * !resync_vpos
                        + AUDIO_OPTS_RESYNC_ADIR * resync_adir + AUDIO_OPTS_RESYNC_ACLIP * resync_aclip
                        + AUDIO_OPTS_LOCKED_FREEZE * freeze_lock + AUDIO_OPTS_LOCKED_PING_PONG * freeze_ping
                        + AUDIO_OPTS_UNLOCK_RESYNC * freeze_resync + AUDIO_OPTS_LOCKED_RESET * alock_reset
                        + AUDIO_OPTS_AUTO_UNLOCK * auto_unlock;
#endif

#ifdef ENABLE_OSC
  uint32_t osc_udp_port = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_osc_udp));
  boolean osc_start = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC_start));
  boolean osc_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC));
#endif

  int rte_keys_virtual = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rte_keys));
  int rte_modes_per_key = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_rte_modes));

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  boolean omc_js_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_js));
  const char *omc_js_fname = lives_entry_get_text(LIVES_ENTRY(prefsw->omc_js_entry));
#endif

#ifdef OMC_MIDI_IMPL
  boolean omc_midi_enable = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_omc_midi));
  const char *omc_midi_fname = lives_entry_get_text(LIVES_ENTRY(prefsw->omc_midi_entry));
  int midicr = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midicr));
  int midirpt = lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(prefsw->spinbutton_midirpt));
  const char *midichan = lives_combo_get_active_text(LIVES_COMBO(prefsw->midichan_combo));

#ifdef ALSA_MIDI
  boolean use_alsa_midi = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi));
  boolean alsa_midi_dummy = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi_dummy));
#endif

#endif
#endif

  boolean pstyle2 = palette->style & STYLE_2;
  boolean pstyle3 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style3));
  boolean pstyle4 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style4));

  char audio_player[256];
  int listlen = lives_list_length(prefs->acodec_list);
  int rec_opts = rec_frames * REC_FRAMES + rec_fps * REC_FPS + rec_effects * REC_EFFECTS + rec_clips * REC_CLIPS + rec_audio *
                 REC_AUDIO + rec_after_pb * REC_AFTER_PB + rec_audio_alock * REC_AUDIO_AUTOLOCK;
  uint64_t warn_mask;

  unsigned char *new_undo_buf;
  LiVESList *ulist;

#ifdef ENABLE_OSC
  boolean set_omc_dev_opts = FALSE;
#ifdef OMC_MIDI_IMPL
  boolean needs_midi_restart = FALSE;
#endif
#endif

  char *tmp;
  char *cdplay_device = lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->cdplay_entry)),
                        -1, NULL, NULL, NULL);

  update_prefs();

  if (prefsw->checkbutton_menu_icons)
    show_menu_icons = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_menu_icons));

#ifdef ENABLE_JACK
#ifndef ENABLE_JACK_TRANSPORT
  jack_opts &= ~(JACK_OPTS_TRANSPORT_SLAVE | JACK_OPTS_TRANSPORT_MASTER
                 | JACK_OPTS_START_TSERVER | JACK_OPTS_TIMEBASE_START
                 | JACK_OPTS_TIMEBASE_SLAVE | JACK_OPTS_TIMEBASE_MASTER
                 | JACK_OPTS_TIMEBASE_LSTART | JACK_OPTS_ENABLE_TCLIENT);
#else
  // ignore transport startup unless the client is enabled
  if (!(jack_opts & JACK_OPTS_ENABLE_TCLIENT)) jack_opts &= ~(JACK_OPTS_START_TSERVER);
#endif
#endif

  lives_snprintf(prefworkdir, PATH_MAX, "%s", prefs->workdir);
  ensure_isdir(prefworkdir);

  // TODO: move all into pref_factory_* functions
  mainw->no_context_update = TRUE;

  if (prefsw->theme_style2)
    pstyle2 = lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->theme_style2));

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_fore), &colf);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_back), &colb);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mabf), &colf2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_mab), &colb2);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infob), &coli);
  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_infot), &colt);

  if (strcasecmp(future_prefs->theme, LIVES_THEME_NONE)) {
    if (!lives_widget_color_equal(&colf, &palette->normal_fore) ||
        !lives_widget_color_equal(&colb, &palette->normal_back) ||
        !lives_widget_color_equal(&colf2, &palette->menu_and_bars_fore) ||
        !lives_widget_color_equal(&colb2, &palette->menu_and_bars) ||
        !lives_widget_color_equal(&colt, &palette->info_text) ||
        !lives_widget_color_equal(&coli, &palette->info_base) ||
        ((pstyle2 * STYLE_2) != (palette->style & STYLE_2)) ||
        ((pstyle3 * STYLE_3) != (palette->style & STYLE_3)) ||
        ((pstyle4 * STYLE_4) != (palette->style & STYLE_4))
       ) {

      lives_widget_color_copy(&palette->normal_fore, &colf);
      lives_widget_color_copy(&palette->normal_back, &colb);
      lives_widget_color_copy(&palette->menu_and_bars_fore, &colf2);
      lives_widget_color_copy(&palette->menu_and_bars, &colb2);
      lives_widget_color_copy(&palette->info_base, &coli);
      lives_widget_color_copy(&palette->info_text, &colt);

      palette->style = STYLE_1 | (pstyle2 * STYLE_2) | (pstyle3 * STYLE_3) | (pstyle4 * STYLE_4);
      mainw->prefs_changed |= PREFS_COLOURS_CHANGED;
    }
  }

  if (pref_factory_color_button(&palette->ce_sel, LIVES_COLOR_BUTTON(prefsw->cbutton_cesel)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->ce_unsel, LIVES_COLOR_BUTTON(prefsw->cbutton_ceunsel)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->frame_surround, LIVES_COLOR_BUTTON(prefsw->cbutton_fsur)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->mt_mark, LIVES_COLOR_BUTTON(prefsw->cbutton_mtmark)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->mt_evbox, LIVES_COLOR_BUTTON(prefsw->cbutton_evbox)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->mt_timeline_reg, LIVES_COLOR_BUTTON(prefsw->cbutton_tlreg)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->vidcol, LIVES_COLOR_BUTTON(prefsw->cbutton_vidcol)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->audcol, LIVES_COLOR_BUTTON(prefsw->cbutton_audcol)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  if (pref_factory_color_button(&palette->fxcol, LIVES_COLOR_BUTTON(prefsw->cbutton_fxcol)))
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcfg), &coltcfg);
  if (!lives_widget_color_equal(&coltcfg, &palette->mt_timecode_fg)) {
    lives_widget_color_copy(&palette->mt_timecode_fg, &coltcfg);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  lives_color_button_get_color(LIVES_COLOR_BUTTON(prefsw->cbutton_tcbg), &coltcbg);
  if (!lives_widget_color_equal(&coltcbg, &palette->mt_timecode_bg)) {
    lives_widget_color_copy(&palette->mt_timecode_bg, &coltcbg);
    mainw->prefs_changed |= PREFS_XCOLOURS_CHANGED;
  }

  if (ARE_PRESENT(encoder_plugins)) {
    audio_codec = lives_combo_get_active_text(LIVES_COMBO(prefsw->acodec_combo));

    for (idx = 0; idx < listlen && lives_strcmp((char *)lives_list_nth_data(prefs->acodec_list, idx), audio_codec); idx++);

    if (idx == listlen) future_prefs->encoder.audio_codec = 0;
    else future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[idx];
  } else future_prefs->encoder.audio_codec = 0;

  lives_snprintf(workdir, PATH_MAX, "%s",
                 (tmp = lives_filename_from_utf8((char *)lives_entry_get_text(LIVES_ENTRY(prefsw->workdir_entry)),
                        -1, NULL, NULL, NULL)));
  lives_free(tmp);

  if (!audp ||
      !strncmp(audp, mainw->string_constants[LIVES_STRING_CONSTANT_NONE],
               strlen(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) lives_snprintf(audio_player, 256, AUDIO_PLAYER_NONE);
  else if (!strncmp(audp, AUDIO_PLAYER_JACK, strlen(AUDIO_PLAYER_JACK))) lives_snprintf(audio_player, 256, AUDIO_PLAYER_JACK);
  else if (!strncmp(audp, AUDIO_PLAYER_PULSE_AUDIO, strlen(AUDIO_PLAYER_PULSE_AUDIO))) lives_snprintf(audio_player, 256,
        AUDIO_PLAYER_PULSE);

  if (!((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) || (prefs->audio_player == AUD_PLAYER_PULSE &&
        capable->has_pulse_audio))) {
    if (prefs->audio_src == AUDIO_SRC_EXT) future_prefs->audio_src = prefs->audio_src = AUDIO_SRC_INT;
  }

  if (rec_opts != prefs->rec_opts) {
    prefs->rec_opts = rec_opts;
    set_int_pref(PREF_RECORD_OPTS, prefs->rec_opts);
  }

  if (!mainw->multitrack) {
    pref_factory_bool(PREF_REC_EXT_AUDIO, rec_ext_audio, TRUE);
  } else {
    future_prefs->audio_src = rec_ext_audio ? AUDIO_SRC_EXT : AUDIO_SRC_INT;
  }

  warn_mask = !warn_fps * WARN_MASK_FPS + !warn_save_set * WARN_MASK_SAVE_SET
              + !warn_fsize * WARN_MASK_FSIZE + !warn_mplayer *
              WARN_MASK_NO_MPLAYER + !warn_missplugs * WARN_MASK_CHECK_PLUGINS + !warn_prefix *
              WARN_MASK_CHECK_PREFIX + !warn_layout_clips * WARN_MASK_LAYOUT_MISSING_CLIPS + !warn_dup_set *
              WARN_MASK_DUPLICATE_SET + !warn_layout_close * WARN_MASK_LAYOUT_CLOSE_FILE + !warn_layout_delete *
              WARN_MASK_LAYOUT_DELETE_FRAMES + !warn_layout_shift * WARN_MASK_LAYOUT_SHIFT_FRAMES + !warn_layout_alter *
              WARN_MASK_LAYOUT_ALTER_FRAMES + !warn_discard_layout * WARN_MASK_EXIT_MT + !warn_after_dvgrab *
              WARN_MASK_AFTER_DVGRAB + !warn_mt_achans * WARN_MASK_MT_ACHANS + !warn_mt_no_jack *
              WARN_MASK_MT_NO_JACK + !warn_layout_adel * WARN_MASK_LAYOUT_DELETE_AUDIO + !warn_layout_ashift *
              WARN_MASK_LAYOUT_SHIFT_AUDIO + !warn_layout_aalt * WARN_MASK_LAYOUT_ALTER_AUDIO + !warn_layout_popup *
              WARN_MASK_LAYOUT_POPUP + +!warn_layout_reload * WARN_MASK_LAYOUT_RELOAD + !warn_yuv4m_open * WARN_MASK_OPEN_YUV4M
              + !warn_mt_backup_space * WARN_MASK_MT_BACKUP_SPACE + !warn_after_crash * WARN_MASK_CLEAN_AFTER_CRASH
              + !warn_no_pulse * WARN_MASK_NO_PULSE_CONNECT + !warn_layout_wipe * WARN_MASK_LAYOUT_WIPE +
              !warn_layout_gamma * WARN_MASK_LAYOUT_GAMMA + !warn_layout_lb * WARN_MASK_LAYOUT_LB + !warn_vjmode_enter *
              WARN_MASK_VJMODE_ENTER + !warn_dmgd_audio * WARN_MASK_DMGD_AUDIO + !warn_dis_dec * WARN_MASK_DISABLED_DECODER;

#ifdef ENABLE_JACK
  warn_mask += !WARN_MASK_JACK_SCRPT * warn_jack_scrpts;
#endif

  if (warn_mask != prefs->warning_mask) {
    prefs->warning_mask = warn_mask;
    set_int64_pref(PREF_LIVES_WARNING_MASK, prefs->warning_mask);
  }

  if (msgs_unlimited) {
    pref_factory_int(PREF_MAX_MSGS, &prefs->max_messages, -max_msgs, TRUE);
  } else {
    pref_factory_int(PREF_MAX_MSGS, &prefs->max_messages, max_msgs, TRUE);
  }

  pref_factory_bool(PREF_SHOW_MSGS, msgs_mbdis, TRUE);
  pref_factory_bool(PREF_MSG_NOPBDIS, msgs_nopbdis, TRUE);
  pref_factory_bool(PREF_MSG_START, show_msgstart, TRUE);
  pref_factory_bool(PREF_SHOW_QUOTA, show_quota, TRUE);
  pref_factory_bool(PREF_AUTOCLEAN_TRASH, autoclean, TRUE);
  pref_factory_bool(PREF_LETTERBOX, lbox, TRUE);
  pref_factory_bool(PREF_LETTERBOX_MT, lboxmt, TRUE);
  pref_factory_bool(PREF_LETTERBOX_ENC, lboxenc, TRUE);
  pref_factory_bool(PREF_NO_LB_GENS, no_lb_gens, TRUE);
  pref_factory_bool(PREF_USE_SCREEN_GAMMA, scgamma, TRUE);
  pref_factory_float(PREF_SCREEN_GAMMA, gamma, TRUE);

  ulist = get_textsizes_list();
  pref_factory_string_choice(PREF_MSG_TEXTSIZE, ulist, msgtextsize, TRUE);
  lives_list_free_all(&ulist);

  if (future_prefs->def_fontstring) {
    lives_freep((void **)&capable->def_fontstring);
    capable->def_fontstring = future_prefs->def_fontstring;
    future_prefs->def_fontstring = NULL;
  }
  pref_factory_utf8(PREF_INTERFACE_FONT, fontname, TRUE);

#ifdef TPLAYWINDOW
  pref_factory_int(PREF_ATRANS_KEY, &prefs->autotrans_key, lives_spin_button_get_value_as_int
                   (LIVES_SPIN_BUTTON(prefsw->spinbutton_atrans_key)), TRUE);
#endif

  if (fsize_to_warn != prefs->warn_file_size) {
    prefs->warn_file_size = fsize_to_warn;
    set_int_pref(PREF_WARN_FILE_SIZE, fsize_to_warn);
  }

  if (dl_bwidth != prefs->dl_bandwidth) {
    prefs->dl_bandwidth = dl_bwidth;
    set_int_pref(PREF_DL_BANDWIDTH_K, dl_bwidth);
  }

  if (ocp != prefs->ocp) {
    prefs->ocp = ocp;
    set_int_pref(PREF_OPEN_COMPRESSION_PERCENT, ocp);
  }

  if (mouse_scroll != prefs->mouse_scroll_clips) {
    prefs->mouse_scroll_clips = mouse_scroll;
    set_boolean_pref(PREF_MOUSE_SCROLL_CLIPS, mouse_scroll);
  }

  pref_factory_bool(PREF_RRCRASH, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_crash)), TRUE);
  pref_factory_bool(PREF_RRSUPER, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_super)), TRUE);
  pref_factory_bool(PREF_RRPRESMOOTH, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_pre_smooth)), TRUE);
  pref_factory_bool(PREF_RRQSMOOTH, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_qsmooth)), TRUE);
  pref_factory_bool(PREF_RRAMICRO, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_amicro)), TRUE);
  pref_factory_bool(PREF_RRRAMICRO, lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->rr_ramicro)), TRUE);

  pref_factory_int(PREF_RRQMODE, &prefs->rr_qmode, lives_combo_get_active_index(LIVES_COMBO(prefsw->rr_combo)), TRUE);
  pref_factory_int(PREF_RRFSTATE, &prefs->rr_fstate, lives_combo_get_active_index(LIVES_COMBO(prefsw->rr_scombo)), TRUE);

  pref_factory_bool(PREF_PUSH_AUDIO_TO_GENS, pa_gens, TRUE);

  pref_factory_bool(PREF_SHOW_BUTTON_ICONS, show_button_icons, TRUE);

  if (prefsw->checkbutton_menu_icons)
    pref_factory_bool(PREF_SHOW_MENU_ICONS, show_menu_icons, TRUE);

  pref_factory_bool(PREF_EXTRA_COLOURS, extra_colours, TRUE);

#ifdef HAVE_PULSE_AUDIO
  pref_factory_bool(PREF_PARESTART,
                    lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart)),
                    TRUE);
  if (prefs->pa_restart)
    pref_factory_string(PREF_PASTARTOPTS, lives_entry_get_text(LIVES_ENTRY(prefsw->audio_command_entry)), TRUE);
#endif

  if (show_asrc != prefs->show_asrc) {
    pref_factory_bool(PREF_SHOW_ASRC, show_asrc, TRUE);
  }

#if GTK_CHECK_VERSION(2, 12, 0)
  if (show_ttips != prefs->show_tooltips) {
    pref_factory_bool(PREF_SHOW_TOOLTIPS, show_ttips, TRUE);
  }
#endif

  if (hfbwnp != prefs->hfbwnp) {
    pref_factory_bool(PREF_HFBWNP, hfbwnp, TRUE);
  }

  if (ce_maxspect != prefs->ce_maxspect) {
    prefs->ce_maxspect = ce_maxspect;
    set_boolean_pref(PREF_CE_MAXSPECT, ce_maxspect);
    if (mainw->multitrack == NULL) {
      if (mainw->current_file > -1) {
        switch_clip(1, mainw->current_file, TRUE);
      }
    }
  }

  if (lives_strcmp(wp_path, prefs->weed_plugin_path)) {
    set_string_pref(PREF_WEED_PLUGIN_PATH, wp_path);
    lives_snprintf(prefs->weed_plugin_path, PATH_MAX, "%s", wp_path);
    lives_setenv("WEED_PLUGIN_PATH", wp_path);
  }

#ifdef HAVE_FREI0R
  if (lives_strcmp(frei0r_path, prefs->frei0r_path)) {
    set_string_pref(PREF_FREI0R_PATH, frei0r_path);
    lives_snprintf(prefs->frei0r_path, PATH_MAX, "%s", frei0r_path);
    lives_setenv("FREI0R_PATH", frei0r_path);
  }
#endif

#ifdef HAVE_LIBVISUAL
  if (lives_strcmp(libvis_path, prefs->libvis_path)) {
    set_string_pref(PREF_LIBVISUAL_PATH, libvis_path);
    lives_snprintf(prefs->libvis_path, PATH_MAX, "%s", libvis_path);
    lives_setenv("VISDUAL_PLUGIN_PATH", libvis_path);
  }
#endif

#ifdef HAVE_LADSPA
  if (lives_strcmp(ladspa_path, prefs->ladspa_path)) {
    set_string_pref(PREF_LADSPA_PATH, ladspa_path);
    lives_snprintf(prefs->ladspa_path, PATH_MAX, "%s", ladspa_path);
    lives_setenv("LADSPA_PATH", ladspa_path);
  }
#endif

  if (lives_strcmp(sepimg_path, mainw->sepimg_path)) {
    lives_snprintf(mainw->sepimg_path, PATH_MAX, "%s", sepimg_path);
    mainw->prefs_changed |= PREFS_IMAGES_CHANGED;
  }

  if (lives_strcmp(frameblank_path, mainw->frameblank_path)) {
    lives_snprintf(mainw->frameblank_path, PATH_MAX, "%s", frameblank_path);
    mainw->prefs_changed |= PREFS_IMAGES_CHANGED;
  }

  ensure_isdir(workdir);

  // disabled_decoders
  if (future_prefs->disabled_decoders
      && lists_differ(prefs->disabled_decoders, future_prefs->disabled_decoders, FALSE)) {
    lives_list_free(prefs->disabled_decoders);
    prefs->disabled_decoders = future_prefs->disabled_decoders;
    future_prefs->disabled_decoders = NULL;
    prefs->disabled_decoders = pref_factory_list(PREF_DISABLED_DECODERS, prefs->disabled_decoders);
  }

  // stop xscreensaver
  if (prefs->stop_screensaver != stop_screensaver) {
    prefs->stop_screensaver = stop_screensaver;
    set_boolean_pref(PREF_STOP_SCREENSAVER, prefs->stop_screensaver);
  }

  // antialias
  if (prefs->antialias != antialias) {
    prefs->antialias = antialias;
    set_boolean_pref(PREF_ANTIALIAS, antialias);
  }

  // load rfx
  if (prefs->load_rfx_builtin != load_rfx) {
    prefs->load_rfx_builtin = load_rfx;
    set_boolean_pref(PREF_LOAD_RFX_BUILTIN, load_rfx);
  }

  // apply gamma correction
  if (prefs->apply_gamma != apply_gamma) {
    prefs->apply_gamma = apply_gamma;
    set_boolean_pref(PREF_APPLY_GAMMA, apply_gamma);
    needs_restart = TRUE;
  }

  // fx_threads
  if (!fx_threads) nfx_threads = 1;
  if (prefs->nfx_threads != nfx_threads) {
    future_prefs->nfx_threads = nfx_threads;
    set_int_pref(PREF_NFX_THREADS, nfx_threads);
  }

  // open maximised
  if (prefs->open_maximised != open_maximised) {
    prefs->open_maximised = open_maximised;
    set_boolean_pref(PREF_OPEN_MAXIMISED, open_maximised);
  }

  // filesel maximised
  if (prefs->fileselmax != fs_maximised) {
    prefs->fileselmax = fs_maximised;
    set_boolean_pref(PREF_FILESEL_MAXIMISED, fs_maximised);
  }

  // monitors

  if (forcesmon != prefs->force_single_monitor) {
    prefs->force_single_monitor = forcesmon;
    set_boolean_pref(PREF_FORCE_SINGLE_MONITOR, forcesmon);
    get_monitors(FALSE);
    if (capable->nmonitors == 0) resize_widgets_for_monitor(TRUE);
  }

  if (capable->nmonitors > 1) {
    if (gui_monitor != prefs->gui_monitor || play_monitor != prefs->play_monitor) {
      char *str = lives_strdup_printf("%d,%d", gui_monitor, play_monitor);
      set_string_pref(PREF_MONITORS, str);
      prefs->gui_monitor = gui_monitor;
      prefs->play_monitor = play_monitor;
      widget_opts.monitor = prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : capable->primary_monitor;
      resize_widgets_for_monitor(TRUE);
    }
  }

  if (ce_thumbs != prefs->ce_thumb_mode) {
    prefs->ce_thumb_mode = ce_thumbs;
    set_boolean_pref(PREF_CE_THUMB_MODE, ce_thumbs);
  }

  // fps stats
  if (prefs->show_player_stats != show_player_stats) {
    prefs->show_player_stats = show_player_stats;
    set_boolean_pref(PREF_SHOW_PLAYER_STATS, show_player_stats);
  }

  if (prefs->stream_audio_out != stream_audio_out) {
    prefs->stream_audio_out = stream_audio_out;
    set_boolean_pref(PREF_STREAM_AUDIO_OUT, stream_audio_out);
  }

  pref_factory_bool(PREF_SHOW_RECENT_FILES, show_recent, TRUE);

  // midi synch
  if (prefs->midisynch != midisynch) {
    prefs->midisynch = midisynch;
    set_boolean_pref(PREF_MIDISYNCH, midisynch);
  }

  // jpeg/png
  if (strcmp(prefs->image_ext, LIVES_FILE_EXT_JPG) && ext_jpeg) {
    set_string_pref(PREF_DEFAULT_IMAGE_TYPE, LIVES_IMAGE_TYPE_JPEG);
    lives_snprintf(prefs->image_ext, 16, LIVES_FILE_EXT_JPG);
    lives_snprintf(prefs->image_type, 16, "%s", LIVES_IMAGE_TYPE_JPEG);
  } else if (!strcmp(prefs->image_ext, LIVES_FILE_EXT_JPG) && !ext_jpeg) {
    set_string_pref(PREF_DEFAULT_IMAGE_TYPE, LIVES_IMAGE_TYPE_PNG);
    lives_snprintf(prefs->image_ext, 16, LIVES_FILE_EXT_PNG);
    lives_snprintf(prefs->image_type, 16, "%s", LIVES_IMAGE_TYPE_PNG);
  }

  // instant open
  if (prefs->instant_open != instant_open) {
    set_boolean_pref(PREF_INSTANT_OPEN, (prefs->instant_open = instant_open));
  }

  // auto deinterlace
  if (prefs->auto_deint != auto_deint) {
    set_boolean_pref(PREF_AUTO_DEINTERLACE, (prefs->auto_deint = auto_deint));
  }

  // auto deinterlace
  if (prefs->auto_trim_audio != auto_trim) {
    set_boolean_pref(PREF_AUTO_TRIM_PAD_AUDIO, (prefs->auto_trim_audio = auto_trim));
  }

  // concat images
  if (prefs->concat_images != concat_images) {
    set_boolean_pref(PREF_CONCAT_IMAGES, (prefs->concat_images = concat_images));
  }

  // encoder
  if (strcmp(prefs->encoder.name, future_prefs->encoder.name)) {
    lives_snprintf(prefs->encoder.name, 64, "%s", future_prefs->encoder.name);
    set_string_pref(PREF_ENCODER, prefs->encoder.name);
    lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
    prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
  }

  // output format
  if (strcmp(prefs->encoder.of_name, future_prefs->encoder.of_name)) {
    lives_snprintf(prefs->encoder.of_name, 64, "%s", future_prefs->encoder.of_name);
    lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", future_prefs->encoder.of_restrict);
    lives_snprintf(prefs->encoder.of_desc, 128, "%s", future_prefs->encoder.of_desc);
    prefs->encoder.of_allowed_acodecs = future_prefs->encoder.of_allowed_acodecs;
    set_string_pref(PREF_OUTPUT_TYPE, prefs->encoder.of_name);
  }

  if (prefs->encoder.audio_codec != future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec = future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec > AUDIO_CODEC_UNKNOWN) {
      set_int64_pref(PREF_ENCODER_ACODEC, prefs->encoder.audio_codec);
    }
  }

  // pb quality
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 0))) pbq = PB_QUALITY_LOW;
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 1))) pbq = PB_QUALITY_MED;
  if (!strcmp(pb_quality, (char *)lives_list_nth_data(prefsw->pbq_list, 2))) pbq = PB_QUALITY_HIGH;

  if (pbq != prefs->pb_quality) {
    future_prefs->pb_quality = prefs->pb_quality = pbq;
    set_int_pref(PREF_PB_QUALITY, pbq);
  }

  pref_factory_bool(PREF_PBQ_ADAPTIVE, pbq_adap, TRUE);

  // video open command
  if (lives_strcmp(prefs->video_open_command, video_open_command)) {
    lives_snprintf(prefs->video_open_command, PATH_MAX * 2, "%s", video_open_command);
    set_string_pref(PREF_VIDEO_OPEN_COMMAND, prefs->video_open_command);
  }

  pref_factory_string(PREF_CMDLINE_ARGS, cmdline_args, TRUE);

  //playback plugin
  set_vpp(TRUE);

  // cd play device
  if (lives_strcmp(prefs->cdplay_device, cdplay_device)) {
    lives_snprintf(prefs->cdplay_device, PATH_MAX, "%s", cdplay_device);
    set_string_pref(PREF_CDPLAY_DEVICE, prefs->cdplay_device);
  }

  lives_free(cdplay_device);

  // default video load directory
  if (lives_strcmp(prefs->def_vid_load_dir, def_vid_load_dir)) {
    lives_snprintf(prefs->def_vid_load_dir, PATH_MAX, "%s/", def_vid_load_dir);
    get_dirname(prefs->def_vid_load_dir);
    set_utf8_pref(PREF_VID_LOAD_DIR, prefs->def_vid_load_dir);
    lives_snprintf(mainw->vid_load_dir, PATH_MAX, "%s", prefs->def_vid_load_dir);
  }

  // default video save directory
  if (lives_strcmp(prefs->def_vid_save_dir, def_vid_save_dir)) {
    lives_snprintf(prefs->def_vid_save_dir, PATH_MAX, "%s/", def_vid_save_dir);
    get_dirname(prefs->def_vid_save_dir);
    set_utf8_pref(PREF_VID_SAVE_DIR, prefs->def_vid_save_dir);
    lives_snprintf(mainw->vid_save_dir, PATH_MAX, "%s", prefs->def_vid_save_dir);
  }

  // default audio directory
  if (lives_strcmp(prefs->def_audio_dir, def_audio_dir)) {
    lives_snprintf(prefs->def_audio_dir, PATH_MAX, "%s/", def_audio_dir);
    get_dirname(prefs->def_audio_dir);
    set_utf8_pref(PREF_AUDIO_DIR, prefs->def_audio_dir);
    lives_snprintf(mainw->audio_dir, PATH_MAX, "%s", prefs->def_audio_dir);
  }

  // default image directory
  if (lives_strcmp(prefs->def_image_dir, def_image_dir)) {
    lives_snprintf(prefs->def_image_dir, PATH_MAX, "%s/", def_image_dir);
    get_dirname(prefs->def_image_dir);
    set_utf8_pref(PREF_IMAGE_DIR, prefs->def_image_dir);
    lives_snprintf(mainw->image_dir, PATH_MAX, "%s", prefs->def_image_dir);
  }

  // default project directory - for backup and restore
  if (lives_strcmp(prefs->def_proj_dir, def_proj_dir)) {
    lives_snprintf(prefs->def_proj_dir, PATH_MAX, "%s/", def_proj_dir);
    get_dirname(prefs->def_proj_dir);
    set_utf8_pref(PREF_PROJ_DIR, prefs->def_proj_dir);
    lives_snprintf(mainw->proj_load_dir, PATH_MAX, "%s", prefs->def_proj_dir);
    lives_snprintf(mainw->proj_save_dir, PATH_MAX, "%s", prefs->def_proj_dir);
  }

  // the theme
  if (lives_utf8_strcmp(future_prefs->theme, theme) && !(!strcasecmp(future_prefs->theme, LIVES_THEME_NONE) &&
      !lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE]))) {
    mainw->prefs_changed |= PREFS_THEME_CHANGED;
    if (lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
      lives_snprintf(prefs->theme, 64, "%s", theme);
      lives_snprintf(future_prefs->theme, 64, "%s", theme);
      set_string_pref(PREF_GUI_THEME, future_prefs->theme);
      widget_opts.apply_theme = 1;
      set_palette_colours(TRUE);
      if (mainw->multitrack) {
        if (mainw->multitrack->frame_pixbuf == mainw->imframe) mainw->multitrack->frame_pixbuf = NULL;
      }
      load_theme_images();
      mainw->prefs_changed |= PREFS_COLOURS_CHANGED | PREFS_IMAGES_CHANGED;
    } else {
      lives_snprintf(future_prefs->theme, 64, LIVES_THEME_NONE);
      set_string_pref(PREF_GUI_THEME, future_prefs->theme);
      delete_pref(THEME_DETAIL_STYLE);
      delete_pref(THEME_DETAIL_SEPWIN_IMAGE);
      delete_pref(THEME_DETAIL_FRAMEBLANK_IMAGE);
      delete_pref(THEME_DETAIL_NORMAL_FORE);
      delete_pref(THEME_DETAIL_NORMAL_BACK);
      delete_pref(THEME_DETAIL_ALT_FORE);
      delete_pref(THEME_DETAIL_ALT_BACK);
      delete_pref(THEME_DETAIL_INFO_TEXT);
      delete_pref(THEME_DETAIL_INFO_BASE);
    }
  }

  // default fps
  if (prefs->default_fps != default_fps) {
    prefs->default_fps = default_fps;
    set_double_pref(PREF_DEFAULT_FPS, prefs->default_fps);
  }

  // ahold
  pref_factory_float(PREF_AHOLD_THRESHOLD, ext_aud_thresh, TRUE);

  // virtual rte keys
  pref_factory_int(PREF_RTE_KEYS_VIRTUAL, &prefs->rte_keys_virtual, rte_keys_virtual, TRUE);

  // virtual rte keys
  pref_factory_int(PREF_RTE_MODES_PERKEY, &prefs->rte_modes_per_key, rte_modes_per_key, TRUE);

  if (ins_speed == prefs->ins_resample) {
    prefs->ins_resample = !ins_speed;
    set_boolean_pref(PREF_INSERT_RESAMPLE, prefs->ins_resample);
  }

  if (ds_warn_level != prefs->ds_warn_level) {
    prefs->ds_warn_level = ds_warn_level;
    mainw->next_ds_warn_level = prefs->ds_warn_level;
    set_int64_pref(PREF_DS_WARN_LEVEL, ds_warn_level);
  }

  if (ds_crit_level != prefs->ds_crit_level) {
    prefs->ds_crit_level = ds_crit_level;
    set_int64_pref(PREF_DS_CRIT_LEVEL, ds_crit_level);
  }

#ifdef ENABLE_OSC
  if (osc_enable) {
    if (prefs->osc_udp_started && osc_udp_port != prefs->osc_udp_port) {
      // port number changed
      lives_osc_end();
      prefs->osc_udp_started = FALSE;
    }
    prefs->osc_udp_port = osc_udp_port;
    // try to start on new port number
    if (!prefs->osc_udp_started) prefs->osc_udp_started = lives_osc_init(prefs->osc_udp_port);
  } else {
    if (prefs->osc_udp_started) {
      lives_osc_end();
      prefs->osc_udp_started = FALSE;
    }
  }
  if (osc_start) {
    if (!future_prefs->osc_start) {
      set_boolean_pref(PREF_OSC_START, TRUE);
      future_prefs->osc_start = TRUE;
    }
  } else {
    if (future_prefs->osc_start) {
      set_boolean_pref(PREF_OSC_START, FALSE);
      future_prefs->osc_start = FALSE;
    }
  }
  if (prefs->osc_udp_port != osc_udp_port) {
    prefs->osc_udp_port = osc_udp_port;
    set_int_pref(PREF_OSC_PORT, osc_udp_port);
  }
#endif

#ifdef RT_AUDIO
  if (prefs->audio_opts != audio_opts) {
    prefs->audio_opts = audio_opts;
    set_int_pref(PREF_AUDIO_OPTS, audio_opts);
  }

  if (rec_desk_audio != prefs->rec_desktop_audio) {
    prefs->rec_desktop_audio = rec_desk_audio;
    set_boolean_pref(PREF_REC_DESKTOP_AUDIO, rec_desk_audio);
  }
#endif

  pref_factory_string(PREF_AUDIO_PLAYER, audio_player, TRUE);

#ifdef ENABLE_JACK
  pref_factory_int(PREF_JACK_OPTS, NULL, jack_opts, TRUE);

  if (jack_acdef) jack_acname = "";

  tmp = lives_strdup(future_prefs->jack_aserver_cfg);
  pref_factory_string(PREF_JACK_ACONFIG, tmp, TRUE);
  lives_free(tmp);
  if (!(future_prefs->jack_opts & JACK_INFO_TEMP_NAMES)) {
    pref_factory_string(PREF_JACK_ACSERVER, jack_acname, TRUE);
    tmp = lives_strdup(future_prefs->jack_aserver_sname);
    pref_factory_string(PREF_JACK_ASSERVER, tmp, TRUE);
    lives_free(tmp);
    tmp = lives_strdup(future_prefs->jack_adriver);
    pref_factory_string(PREF_JACK_ADRIVER, tmp, TRUE);
    lives_free(tmp);
  }
#ifdef ENABLE_JACK_TRANSPORT
  if (jack_tcdef) jack_tcname = "";

  if (future_prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
    pref_factory_string(PREF_JACK_TCONFIG, future_prefs->jack_tserver_cfg, TRUE);
    if (!(future_prefs->jack_opts & JACK_INFO_TEMP_NAMES)) {
      pref_factory_string(PREF_JACK_TCSERVER, jack_tcname, TRUE);
      tmp = lives_strdup(future_prefs->jack_tserver_sname);
      pref_factory_string(PREF_JACK_TSSERVER, tmp, TRUE);
      lives_free(tmp);
      tmp = lives_strdup(future_prefs->jack_tdriver);
      pref_factory_string(PREF_JACK_TDRIVER, tmp, TRUE);
      lives_free(tmp);
    }
  }
#endif
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  pref_factory_utf8(PREF_OMC_JS_FNAME, omc_js_fname, TRUE);
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_JS, omc_js_enable, FALSE))
    set_omc_dev_opts = TRUE;
#endif

#ifdef OMC_MIDI_IMPL
  pref_factory_string(PREF_MIDI_RCV_CHANNEL, midichan, TRUE);
  pref_factory_utf8(PREF_OMC_MIDI_FNAME, omc_midi_fname, TRUE);

  pref_factory_int(PREF_MIDI_CHECK_RATE, &prefs->midi_check_rate, midicr, TRUE);
  pref_factory_int(PREF_MIDI_RPT, &prefs->midi_rpt, midirpt, TRUE);

  if (omc_midi_enable && !(prefs->omc_dev_opts & OMC_DEV_MIDI)) needs_midi_restart = TRUE;
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_MIDI, omc_midi_enable, FALSE))
    set_omc_dev_opts = TRUE;

#ifdef ALSA_MIDI
  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_FORCE_RAW_MIDI, !use_alsa_midi, FALSE))
    set_omc_dev_opts = TRUE;

  if (use_alsa_midi == ((prefs->omc_dev_opts & OMC_DEV_FORCE_RAW_MIDI) / OMC_DEV_FORCE_RAW_MIDI)) {
    if (!needs_midi_restart) {
      needs_midi_restart = (mainw->ext_cntl[EXT_CNTL_MIDI]);
    }
  }

  if (pref_factory_bitmapped(PREF_OMC_DEV_OPTS, OMC_DEV_MIDI_DUMMY, alsa_midi_dummy, FALSE)) {
    set_omc_dev_opts = TRUE;
    if (!needs_midi_restart) {
      needs_midi_restart = (mainw->ext_cntl[EXT_CNTL_MIDI]);
    }
  }
#endif

  if (needs_midi_restart) {
    midi_close();
    midi_open();
  }

#endif
  if (set_omc_dev_opts) set_int_pref(PREF_OMC_DEV_OPTS, prefs->omc_dev_opts);
#endif

  if (mt_enter_prompt != prefs->mt_enter_prompt) {
    prefs->mt_enter_prompt = mt_enter_prompt;
    set_boolean_pref(PREF_MT_ENTER_PROMPT, mt_enter_prompt);
  }

  pref_factory_bool(PREF_MT_EXIT_RENDER, mt_exit_render, TRUE);

  if (render_prompt != prefs->render_prompt) {
    prefs->render_prompt = render_prompt;
    set_boolean_pref(PREF_RENDER_PROMPT, render_prompt);
  }

  if (mt_pertrack_audio != prefs->mt_pertrack_audio) {
    prefs->mt_pertrack_audio = mt_pertrack_audio;
    set_boolean_pref(PREF_MT_PERTRACK_AUDIO, mt_pertrack_audio);
  }

  if (mt_backaudio != prefs->mt_backaudio) {
    prefs->mt_backaudio = mt_backaudio;
    set_int_pref(PREF_MT_BACKAUDIO, mt_backaudio);
  }

  if (mt_def_width != prefs->mt_def_width) {
    prefs->mt_def_width = mt_def_width;
    set_int_pref(PREF_MT_DEF_WIDTH, mt_def_width);
  }
  if (mt_def_height != prefs->mt_def_height) {
    prefs->mt_def_height = mt_def_height;
    set_int_pref(PREF_MT_DEF_HEIGHT, mt_def_height);
  }
  if (mt_def_fps != prefs->mt_def_fps) {
    prefs->mt_def_fps = mt_def_fps;
    set_double_pref(PREF_MT_DEF_FPS, mt_def_fps);
  }
  if (!mt_enable_audio) mt_def_achans = 0;
  if (mt_def_achans != prefs->mt_def_achans) {
    prefs->mt_def_achans = mt_def_achans;
    set_int_pref(PREF_MT_DEF_ACHANS, mt_def_achans);
  }
  if (mt_def_asamps != prefs->mt_def_asamps) {
    prefs->mt_def_asamps = mt_def_asamps;
    set_int_pref(PREF_MT_DEF_ASAMPS, mt_def_asamps);
  }
  if (mt_def_arate != prefs->mt_def_arate) {
    prefs->mt_def_arate = mt_def_arate;
    set_int_pref(PREF_MT_DEF_ARATE, mt_def_arate);
  }
  if (mt_def_signed_endian != prefs->mt_def_signed_endian) {
    prefs->mt_def_signed_endian = mt_def_signed_endian;
    set_int_pref(PREF_MT_DEF_SIGNED_ENDIAN, mt_def_signed_endian);
  }

  if (mt_undo_buf != prefs->mt_undo_buf) {
    if ((new_undo_buf = (unsigned char *)lives_malloc(mt_undo_buf * 1024 * 1024)) == NULL) {
      do_mt_set_mem_error(mainw->multitrack != NULL);
    } else {
      if (mainw->multitrack) {
        if (mainw->multitrack->undo_mem) {
          if (mt_undo_buf < prefs->mt_undo_buf) {
            ssize_t space_needed = mainw->multitrack->undo_buffer_used - (size_t)(mt_undo_buf * 1024 * 1024);
            if (space_needed > 0) make_backup_space(mainw->multitrack, space_needed);
            lives_memcpy(new_undo_buf, mainw->multitrack->undo_mem, mt_undo_buf * 1024 * 1024);
          } else lives_memcpy(new_undo_buf, mainw->multitrack->undo_mem, prefs->mt_undo_buf * 1024 * 1024);
          ulist = mainw->multitrack->undos;
          while (ulist) {
            ulist->data = new_undo_buf + ((unsigned char *)ulist->data - mainw->multitrack->undo_mem);
            ulist = ulist->next;
          }
          lives_free(mainw->multitrack->undo_mem);
          mainw->multitrack->undo_mem = new_undo_buf;
        } else {
          mainw->multitrack->undo_mem = (unsigned char *)lives_malloc(mt_undo_buf * 1024 * 1024);
          if (mainw->multitrack->undo_mem == NULL) {
            do_mt_set_mem_error(TRUE);
          } else {
            mainw->multitrack->undo_buffer_used = 0;
            mainw->multitrack->undos = NULL;
            mainw->multitrack->undo_offset = 0;
          }
        }
      }
      prefs->mt_undo_buf = mt_undo_buf;
      set_int_pref(PREF_MT_UNDO_BUF, mt_undo_buf);
    }
  }

  if (mt_autoback_always) mt_autoback_time = 0;
  else if (mt_autoback_never) mt_autoback_time = -1;

  pref_factory_int(PREF_MT_AUTO_BACK, &prefs->mt_auto_back, mt_autoback_time, TRUE);

  if (max_disp_vtracks != prefs->max_disp_vtracks) {
    prefs->max_disp_vtracks = max_disp_vtracks;
    set_int_pref(PREF_MAX_DISP_VTRACKS, max_disp_vtracks);
    if (mainw->multitrack) scroll_tracks(mainw->multitrack, mainw->multitrack->top_track, FALSE);
  }

  if (startup_ce && future_prefs->startup_interface != STARTUP_CE) {
    future_prefs->startup_interface = STARTUP_CE;
    set_int_pref(PREF_STARTUP_INTERFACE, STARTUP_CE);
    if ((mainw->multitrack && mainw->multitrack->event_list) || mainw->stored_event_list)
      write_backup_layout_numbering(mainw->multitrack);
  } else if (!startup_ce && future_prefs->startup_interface != STARTUP_MT) {
    future_prefs->startup_interface = STARTUP_MT;
    set_int_pref(PREF_STARTUP_INTERFACE, STARTUP_MT);
    if ((mainw->multitrack && mainw->multitrack->event_list) || mainw->stored_event_list)
      write_backup_layout_numbering(mainw->multitrack);
  }

  mainw->no_context_update = FALSE;

  if (mainw->has_session_workdir) {
    if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_perm_workdir))) {
      mainw->has_session_workdir = FALSE;
      set_string_pref_priority(PREF_WORKING_DIR, prefs->workdir);
      set_string_pref(PREF_WORKING_DIR_OLD, prefs->workdir);
    }
  }

  if (lives_strcmp(prefworkdir, workdir)) {
    char *xworkdir = lives_strdup(workdir);
    if (check_workdir_valid(&xworkdir, LIVES_DIALOG(prefsw->prefs_dialog), FALSE) == LIVES_RESPONSE_OK) {
      if (workdir_change_dialog()) {
        lives_snprintf(workdir, PATH_MAX, "%s", xworkdir);
        set_workdir_label_text(LIVES_LABEL(prefsw->workdir_label), xworkdir);
        lives_free(xworkdir);

        lives_widget_queue_draw(prefsw->workdir_label);
        lives_widget_context_update(); // update prefs window before showing confirmation box
        lives_snprintf(future_prefs->workdir, PATH_MAX, "%s", workdir);
        mainw->prefs_changed = PREFS_WORKDIR_CHANGED;
        needs_restart = TRUE;
      } else {
        future_prefs->workdir[0] = '\0';
        mainw->prefs_changed |= PREFS_NEEDS_REVERT;
      }
    }
  }

  return needs_restart;
}


void save_future_prefs(void) {
  // save future prefs on exit, if they have changed

  // show_recent is a special case, future prefs has our original value
  if (!prefs->show_recent && future_prefs->show_recent) {
    for (int i = 1; i < + N_RECENT_FILES; i++)  {
      char *prefname = lives_strdup_printf("%s%d", PREF_RECENT, i);
      set_string_pref(prefname, "");
      lives_free(prefname);
    }
  }

  if (prefs->pref_trash != future_prefs->pref_trash) {
    set_boolean_pref(PREF_PREF_TRASH, prefs->pref_trash);
  }

  if (*future_prefs->workdir) {
    set_string_pref_priority(PREF_WORKING_DIR, future_prefs->workdir);
    set_string_pref(PREF_WORKING_DIR_OLD, future_prefs->workdir);
  }
}


void rdet_acodec_changed(LiVESCombo * acodec_combo, livespointer user_data) {
  int listlen = lives_list_length(prefs->acodec_list);
  int idx;
  const char *audio_codec = lives_combo_get_active_text(acodec_combo);
  if (!strcmp(audio_codec, mainw->string_constants[LIVES_STRING_CONSTANT_ANY])) return;

  for (idx = 0; idx < listlen && lives_strcmp((char *)lives_list_nth_data(prefs->acodec_list, idx), audio_codec); idx++);

  if (idx == listlen) future_prefs->encoder.audio_codec = 0;
  else future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[idx];

  if (prefs->encoder.audio_codec != future_prefs->encoder.audio_codec) {
    prefs->encoder.audio_codec = future_prefs->encoder.audio_codec;
    if (prefs->encoder.audio_codec > AUDIO_CODEC_UNKNOWN) {
      set_int64_pref(PREF_ENCODER_ACODEC, prefs->encoder.audio_codec);
    }
  }
}


void set_acodec_list_from_allowed(_prefsw * prefsw, render_details * rdet) {
  // could be done better, but no time...
  // get settings for current format

  int count = 0, idx;
  boolean is_allowed = FALSE;

  if (prefs->acodec_list) {
    lives_list_free(prefs->acodec_list);
    prefs->acodec_list = NULL;
  }

  if (future_prefs->encoder.of_allowed_acodecs == 0) {
    prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));
    future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[0] = AUDIO_CODEC_NONE;

    if (prefsw) {
      lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), 0);
    }
    if (rdet) {
      lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
      lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), 0);
    }
    return;
  }
  for (idx = 0; strlen(anames[idx]); idx++) {
    if (future_prefs->encoder.of_allowed_acodecs & ((uint64_t)1 << idx)) {
      if (idx == AUDIO_CODEC_PCM - 1) prefs->acodec_list = lives_list_append(prefs->acodec_list,
            (_("PCM (highest quality; largest files)")));
      else prefs->acodec_list = lives_list_append(prefs->acodec_list, lives_strdup(anames[idx]));
      prefs->acodec_list_to_format[count++] = idx;
      if (future_prefs->encoder.audio_codec == idx) is_allowed = TRUE;
    }
  }

  if (prefsw) {
    lives_combo_populate(LIVES_COMBO(prefsw->acodec_combo), prefs->acodec_list);
  }
  if (rdet) {
    lives_combo_populate(LIVES_COMBO(rdet->acodec_combo), prefs->acodec_list);
  }
  if (!is_allowed) {
    future_prefs->encoder.audio_codec = prefs->acodec_list_to_format[0];
  }

  for (idx = 0; idx < lives_list_length(prefs->acodec_list); idx++) {
    if (prefs->acodec_list_to_format[idx] == future_prefs->encoder.audio_codec) {
      if (prefsw) {
        lives_combo_set_active_index(LIVES_COMBO(prefsw->acodec_combo), idx);
      }
      if (rdet) {
        lives_combo_set_active_index(LIVES_COMBO(rdet->acodec_combo), idx);
      }
      break;
    }
  }
}


void after_vpp_changed(LiVESWidget * vpp_combo, livespointer advbutton) {
  const char *newvpp = lives_combo_get_active_text(LIVES_COMBO(vpp_combo));
  _vid_playback_plugin *tmpvpp;

  if (!lives_utf8_strcasecmp(newvpp, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), FALSE);
  } else {
    lives_widget_set_sensitive(LIVES_WIDGET(advbutton), TRUE);

    // will call set_astream_settings
    if ((tmpvpp = open_vid_playback_plugin(newvpp, FALSE)) == NULL) {
      lives_combo_set_active_string(LIVES_COMBO(vpp_combo), mainw->vpp->soname);
      return;
    }
    close_vid_playback_plugin(tmpvpp);
  }
  lives_snprintf(future_prefs->vpp_name, 64, "%s", newvpp);

  if (future_prefs->vpp_argv) {
    int i;
    for (i = 0; future_prefs->vpp_argv[i]; lives_free(future_prefs->vpp_argv[i++]));
    lives_free(future_prefs->vpp_argv);
    future_prefs->vpp_argv = NULL;
  }
  future_prefs->vpp_argc = 0;

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
}


static void on_forcesmon_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->spinbutton_gmoni, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->spinbutton_pmoni, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(prefsw->ce_thumbs, !lives_toggle_button_get_active(tbutton) &&
                             play_monitor != gui_monitor &&
                             play_monitor != 0 && capable->nmonitors > 1);
}


static void pmoni_gmoni_changed(LiVESWidget * sbut, livespointer user_data) {
  _prefsw *prefsw = (_prefsw *)user_data;
  int gui_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_gmoni));
  int play_monitor = lives_spin_button_get_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_pmoni));
  lives_widget_set_sensitive(prefsw->ce_thumbs, play_monitor != gui_monitor &&
                             play_monitor != 0 && !lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->forcesmon)) &&
                             capable->nmonitors > 1);
}


static void on_mtbackevery_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data) xprefsw = (_prefsw *)user_data;
  else xprefsw = prefsw;
  lives_widget_set_sensitive(xprefsw->spinbutton_mt_ab_time, lives_toggle_button_get_active(tbutton));

}


#ifdef ENABLE_JACK_TRANSPORT
static void after_jack_client_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start), FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_START) ? TRUE : FALSE);
  }
}

static void after_jack_tb_start_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client), FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_client),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE) ? TRUE : FALSE);
  }
}

static void after_jack_master_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_start), FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_start),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART) ? TRUE : FALSE);
  }
}

static void after_jack_upd_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  if (!lives_toggle_button_get_active(tbutton)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_update), FALSE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_update),
                                   (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_MASTER) ? TRUE : FALSE);
  }
}
#endif


#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifdef ALSA_MIDI
static void on_alsa_midi_toggled(LiVESToggleButton * tbutton, livespointer user_data) {
  _prefsw *xprefsw;

  if (user_data) xprefsw = (_prefsw *)user_data;
  else xprefsw = prefsw;

  lives_widget_set_sensitive(xprefsw->button_midid, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->alsa_midi_dummy, lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->omc_midi_entry, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midicr, !lives_toggle_button_get_active(tbutton));
  lives_widget_set_sensitive(xprefsw->spinbutton_midirpt, !lives_toggle_button_get_active(tbutton));
}
#endif
#endif
#endif


static void on_audp_entry_changed(LiVESWidget * audp_combo, livespointer ptr) {
  const char *audp = lives_combo_get_active_text(LIVES_COMBO(audp_combo));

  if (!(*audp) || !strcmp(audp, prefsw->audp_name)) return;
  if (LIVES_IS_PLAYING) {
    do_aud_during_play_error();
    lives_signal_handler_block(audp_combo, prefsw->audp_entry_func);

    lives_combo_set_active_string(LIVES_COMBO(audp_combo), prefsw->audp_name);

    //lives_widget_queue_draw(audp_entry);
    lives_signal_handler_unblock(audp_combo, prefsw->audp_entry_func);
    return;
  }

#ifdef RT_AUDIO
  if (!strcmp(audp, AUDIO_PLAYER_JACK) || !strncmp(audp, AUDIO_PLAYER_PULSE_AUDIO, strlen(AUDIO_PLAYER_PULSE_AUDIO))) {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips, TRUE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow, TRUE);
    lives_widget_set_sensitive(prefsw->resync_fps, TRUE);
    lives_widget_set_sensitive(prefsw->resync_vpos, TRUE);
    lives_widget_set_sensitive(prefsw->resync_adir, TRUE);
    lives_widget_set_sensitive(prefsw->resync_aclip, TRUE);
    lives_widget_set_sensitive(prefsw->raudio, TRUE);
    lives_widget_set_sensitive(prefsw->raudio_alock, TRUE);
    lives_widget_set_sensitive(prefsw->pa_gens, TRUE);
    lives_widget_set_sensitive(prefsw->rextaudio, TRUE);
  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_aclips, FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_afollow, FALSE);
    lives_widget_set_sensitive(prefsw->resync_fps, FALSE);
    lives_widget_set_sensitive(prefsw->resync_vpos, FALSE);
    lives_widget_set_sensitive(prefsw->resync_adir, FALSE);
    lives_widget_set_sensitive(prefsw->resync_aclip, FALSE);
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
    lives_widget_set_sensitive(prefsw->raudio_alock, FALSE);
    lives_widget_set_sensitive(prefsw->pa_gens, FALSE);
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), FALSE);
  }
#ifdef ENABLE_JACK
  if (!strcmp(audp, AUDIO_PLAYER_JACK)) {
    lives_widget_set_sensitive(prefsw->jack_apstats, TRUE);
    lives_widget_set_sensitive(prefsw->jack_aplayout, TRUE);
    lives_widget_set_sensitive(prefsw->jack_aplayout2, TRUE);
    hide_warn_image(prefsw->jack_aplabel);
    lives_widget_set_no_show_all(prefsw->jack_int_label, FALSE);
    lives_widget_show_all(prefsw->jack_int_label);
  } else {
    lives_widget_set_sensitive(prefsw->jack_apstats, FALSE);
    lives_widget_set_sensitive(prefsw->jack_aplayout, FALSE);
    lives_widget_set_sensitive(prefsw->jack_aplayout2, FALSE);
    lives_widget_set_no_show_all(prefsw->jack_int_label, TRUE);
    lives_widget_hide(prefsw->jack_int_label);
    show_warn_image(prefsw->jack_aplabel, NULL);
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (!strncmp(audp, AUDIO_PLAYER_PULSE_AUDIO, strlen(AUDIO_PLAYER_PULSE_AUDIO))) {
    lives_widget_set_sensitive(prefsw->checkbutton_parestart, TRUE);
    lives_widget_set_sensitive(prefsw->audio_command_entry,
                               lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart)));

  } else {
    lives_widget_set_sensitive(prefsw->checkbutton_parestart, FALSE);
    lives_widget_set_sensitive(prefsw->audio_command_entry, FALSE);
  }
#endif
#endif
  lives_free(prefsw->audp_name);

  prefsw->audp_name = lives_strdup(lives_combo_get_active_text(LIVES_COMBO(audp_combo)));
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
}


static void stream_audio_toggled(LiVESToggleButton * togglebutton, livespointer user_data) {
  // if audio streaming is enabled, check requisites

  if (lives_toggle_button_get_active(togglebutton)) {
    // init vpp, get audio codec, check requisites
    _vid_playback_plugin *tmpvpp;
    uint64_t orig_acodec = AUDIO_CODEC_NONE;

    if (*future_prefs->vpp_name) {
      if ((tmpvpp = open_vid_playback_plugin(future_prefs->vpp_name, FALSE)) == NULL) return;
    } else {
      tmpvpp = mainw->vpp;
      orig_acodec = mainw->vpp->audio_codec;
      get_best_audio(mainw->vpp); // check again because audio player may differ
    }

    if (tmpvpp->audio_codec != AUDIO_CODEC_NONE) {
      // make audiostream plugin name
      size_t rlen;

      char buf[1024];
      char *com;

      char *astreamer = lives_build_filename(prefs->lib_dir, PLUGIN_EXEC_DIR, PLUGIN_AUDIO_STREAM, AUDIO_STREAMER_NAME, NULL);

      com = lives_strdup_printf("\"%s\" check %lu", astreamer, tmpvpp->audio_codec);
      lives_free(astreamer);

      rlen = lives_popen(com, TRUE, buf, 1024);
      lives_free(com);
      if (rlen > 0) {
        lives_toggle_button_set_active(togglebutton, FALSE);
      }
    }

    if (tmpvpp) {
      if (tmpvpp != mainw->vpp) {
        // close the temp current vpp
        close_vid_playback_plugin(tmpvpp);
      } else {
        // restore current codec
        mainw->vpp->audio_codec = orig_acodec;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void prefsw_set_astream_settings(_vid_playback_plugin * vpp, _prefsw * prefsw) {
  if (vpp && (vpp->audio_codec != AUDIO_CODEC_NONE || vpp->init_audio)) {
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio, TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_stream_audio), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_stream_audio, FALSE);
  }
}


void prefsw_set_rec_after_settings(_vid_playback_plugin * vpp, _prefsw * prefsw) {
  if (vpp && (vpp->capabilities & VPP_CAN_RETURN)) {
    /// TODO !!!
    //lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb, TRUE);
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON (prefsw->checkbutton_stream_audio),future_prefs->stream_audio_out);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_rec_after_pb), FALSE);
    lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb, FALSE);
  }
}


/*
  Initialize preferences dialog list
*/
static void pref_init_list(LiVESWidget * list) {
  LiVESCellRenderer *renderer, *pixbufRenderer;
  LiVESTreeViewColumn *column1, *column2;
  LiVESListStore *store;

  renderer = lives_cell_renderer_text_new();
  pixbufRenderer = lives_cell_renderer_pixbuf_new();

  column1 = lives_tree_view_column_new_with_attributes("List Icons", pixbufRenderer, LIVES_TREE_VIEW_COLUMN_PIXBUF, LIST_ICON,
            NULL);
  column2 = lives_tree_view_column_new_with_attributes("List Items", renderer, LIVES_TREE_VIEW_COLUMN_TEXT, LIST_ITEM, NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column1);
  lives_tree_view_append_column(LIVES_TREE_VIEW(list), column2);
  lives_tree_view_column_set_sizing(column2, LIVES_TREE_VIEW_COLUMN_FIXED);
  lives_tree_view_column_set_fixed_width(column2, 150. * widget_opts.scaleW);

  store = lives_list_store_new(N_COLUMNS, LIVES_COL_TYPE_PIXBUF, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_UINT);

  lives_tree_view_set_model(LIVES_TREE_VIEW(list), LIVES_TREE_MODEL(store));
}


/*
  Adds entry to preferences dialog list
*/
static void prefs_add_to_list(LiVESWidget * list, LiVESPixbuf * pix, const char *str, uint32_t idx) {
  LiVESListStore *store;
  LiVESTreeIter iter;

  char *tmp = lives_strdup_printf("\n  %s\n", str);

  store = LIVES_LIST_STORE(lives_tree_view_get_model(LIVES_TREE_VIEW(list)));

  lives_list_store_insert(store, &iter, idx);
  lives_list_store_set(store, &iter, LIST_ICON, pix, LIST_ITEM, tmp, LIST_NUM, idx, -1);
  lives_free(tmp);
}


/*
  Callback function called when preferences list row changed
*/
void on_prefs_page_changed(LiVESTreeSelection * widget, _prefsw * prefsw) {
  LiVESTreeIter iter;
  LiVESTreeModel *model;
  //LiVESListStore *store;
  char *name, *tmp;

  for (int i = 0; i < 2; i++) {
    // for some reason gtk+ needs us to do this twice..
    if (lives_tree_selection_get_selected(widget, &model, &iter)) {

      // Hide currently shown widget
      if (prefsw->right_shown) {
        lives_widget_hide(prefsw->right_shown);
      }

      switch (prefs_current_page) {
      case LIST_ENTRY_RESET:
        lives_widget_show_all(prefsw->vbox_right_reset);
        prefsw->right_shown = prefsw->vbox_right_reset;
        break;
      case LIST_ENTRY_MULTITRACK:
        lives_widget_show_all(prefsw->scrollw_right_multitrack);
        prefsw->right_shown = prefsw->scrollw_right_multitrack;
        break;
      case LIST_ENTRY_DECODING:
        lives_widget_show_all(prefsw->scrollw_right_decoding);
        prefsw->right_shown = prefsw->scrollw_right_decoding;
        break;
      case LIST_ENTRY_PLAYBACK:
        lives_widget_show_all(prefsw->scrollw_right_playback);
        prefsw->right_shown = prefsw->scrollw_right_playback;
        break;
      case LIST_ENTRY_RECORDING:
        lives_widget_show_all(prefsw->scrollw_right_recording);
        prefsw->right_shown = prefsw->scrollw_right_recording;
        break;
      case LIST_ENTRY_ENCODING:
        lives_widget_show_all(prefsw->scrollw_right_encoding);
        prefsw->right_shown = prefsw->scrollw_right_encoding;
        break;
      case LIST_ENTRY_EFFECTS:
        lives_widget_show_all(prefsw->scrollw_right_effects);
        prefsw->right_shown = prefsw->scrollw_right_effects;
        break;
      case LIST_ENTRY_DIRECTORIES:
        lives_widget_show_all(prefsw->scrollw_right_directories);
        prefsw->right_shown = prefsw->scrollw_right_directories;
        break;
      case LIST_ENTRY_WARNINGS:
        lives_widget_show_all(prefsw->scrollw_right_warnings);
        prefsw->right_shown = prefsw->scrollw_right_warnings;
        break;
      case LIST_ENTRY_MISC:
        lives_widget_show_all(prefsw->scrollw_right_misc);
        prefsw->right_shown = prefsw->scrollw_right_misc;
        if (!check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)
            && !check_for_executable(&capable->has_icedax, EXEC_ICEDAX)) {
          lives_widget_hide(prefsw->cdda_hbox);
        }
        break;
      case LIST_ENTRY_THEMES:
        lives_widget_show_all(prefsw->scrollw_right_themes);
        prefsw->right_shown = prefsw->scrollw_right_themes;
        break;
      case LIST_ENTRY_NET:
        lives_widget_show_all(prefsw->scrollw_right_net);
        prefsw->right_shown = prefsw->scrollw_right_net;
        break;
      case LIST_ENTRY_JACK:
        lives_widget_show_all(prefsw->scrollw_right_jack);

#ifdef ENABLE_JACK
        if (prefs->audio_player == AUD_PLAYER_JACK) {
          lives_widget_show(prefsw->jack_int_label);
        }
#endif

        prefsw->right_shown = prefsw->scrollw_right_jack;
        break;
      case LIST_ENTRY_MIDI:
        lives_widget_show_all(prefsw->scrollw_right_midi);
        prefsw->right_shown = prefsw->scrollw_right_midi;
#ifdef OMC_MIDI_IMPL
#ifndef ALSA_MIDI
        lives_widget_hide(prefsw->midi_hbox);
#endif
#endif
        break;
      case LIST_ENTRY_GUI:
      default:
        lives_widget_show_all(prefsw->scrollw_right_gui);
        prefsw->right_shown = prefsw->scrollw_right_gui;
        if (nmons > 1) {
          lives_widget_set_no_show_all(prefsw->forcesmon_hbox, FALSE);
          lives_widget_show(prefsw->forcesmon_hbox);
#if !LIVES_HAS_GRID_WIDGET
          lives_widget_set_no_show_all(prefsw->ce_thumbs, FALSE);
          lives_widget_show(prefsw->ce_thumbs);
#endif
        }
        prefs_current_page = LIST_ENTRY_GUI;
      }
      // get all for current row
      lives_tree_model_get(model, &iter, LIST_NUM, &prefs_current_page, LIST_ITEM, &name, -1);
      tmp = lives_strdup_printf("<big><b>%s</b></big>", name);
      widget_opts.use_markup = TRUE;
      lives_label_set_text(LIVES_LABEL(prefsw->tlabel), tmp);
      widget_opts.use_markup = FALSE;
      lives_free(tmp);
      /// TODO - highlight the selected row in (LiSTStore *)
    }
  }

  lives_widget_queue_draw(prefsw->prefs_dialog);
}


/*
  Function makes apply button sensitive
*/
void apply_button_set_enabled(LiVESWidget * widget, livespointer func_data) {
  if (!prefsw || prefsw->ignore_apply) return;
  lives_button_grab_default_special(prefsw->applybutton); // need to do this first or the button doesn't get its colour
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->revertbutton), TRUE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), FALSE);
#ifdef ENABLE_JACK
  if ((future_prefs->jack_opts & JACK_INFO_TEMP_OPTS)
      && !(future_prefs->jack_opts & JACK_INFO_TEMP_OPTS))
    lives_widget_set_sensitive(prefsw->jack_apperm, FALSE);
#endif
}


static void spinbutton_ds_value_changed(LiVESSpinButton * warn_ds, livespointer is_critp) {
  boolean is_crit = LIVES_POINTER_TO_INT(is_critp);
  char *tmp = NULL, *tmp2;
  double myval = lives_spin_button_get_value(warn_ds);
  uint64_t umyval = (uint64_t)myval * MILLIONS(1);
  if (is_crit)
    lives_spin_button_set_range(LIVES_SPIN_BUTTON(prefsw->spinbutton_warn_ds), myval, DS_WARN_CRIT_MAX);
  if (myval == 0.) tmp2 = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_DISABLED]);
  else {
    tmp = lives_format_storage_space_string(umyval);
    tmp2 = lives_strdup_printf("(%s)", tmp);
  }
  if (is_crit)
    lives_label_set_text(LIVES_LABEL(prefsw->dsc_label), tmp2);
  else
    lives_label_set_text(LIVES_LABEL(prefsw->dsl_label), tmp2);
  if (tmp)lives_free(tmp);
  lives_free(tmp2);
}


static void theme_widgets_set_sensitive(LiVESCombo * combo, livespointer xprefsw) {
  _prefsw *prefsw = (_prefsw *)xprefsw;
  const char *theme = lives_combo_get_active_text(combo);
  boolean theme_set = TRUE;
  if (!lives_utf8_strcmp(theme, mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) theme_set = FALSE;
  lives_widget_set_sensitive(prefsw->cbutton_fxcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_audcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_vidcol, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_evbox, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_ceunsel, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_cesel, theme_set);
  lives_widget_set_sensitive(prefsw->fb_filebutton, theme_set);
  lives_widget_set_sensitive(prefsw->sepimg_entry, theme_set);
  lives_widget_set_sensitive(prefsw->se_filebutton, theme_set);
  lives_widget_set_sensitive(prefsw->frameblank_entry, theme_set);
  lives_widget_set_sensitive(prefsw->theme_style4, theme_set);
  if (prefsw->theme_style2) {
    lives_widget_set_sensitive(prefsw->theme_style2, theme_set);
  }
  lives_widget_set_sensitive(prefsw->theme_style3, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_back, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_fore, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mab, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mabf, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infot, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infob, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_infot, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_mtmark, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tlreg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tcfg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_tcbg, theme_set);
  lives_widget_set_sensitive(prefsw->cbutton_fsur, theme_set);
}


#if GTK_CHECK_VERSION(3, 2, 0)
static void font_preview_clicked(LiVESButton * button, LiVESFontChooser * fontbutton) {
  if (GET_INT_DATA(button, PREVIEW_KEY)) {
    char *fontname = lives_font_chooser_get_font(fontbutton);
    lives_button_set_label(button, "_Revert to previous font");
    lives_button_set_image_from_stock(button, LIVES_STOCK_UNDO);
    future_prefs->def_fontstring = capable->def_fontstring;
    capable->def_fontstring = NULL;
    pref_factory_utf8(PREF_INTERFACE_FONT, fontname, FALSE);
    SET_INT_DATA(button, PREVIEW_KEY, FALSE);
  } else {
    lives_button_set_label(button, "_Preview font");
    lives_button_set_image_from_stock(button, LIVES_STOCK_APPLY);
    if (future_prefs->def_fontstring) {
      lives_freep((void **)&capable->def_fontstring);
      capable->def_fontstring = future_prefs->def_fontstring;
      future_prefs->def_fontstring = NULL;
      pref_factory_utf8(PREF_INTERFACE_FONT, capable->def_fontstring, FALSE);
    } else lives_widget_set_sensitive(LIVES_WIDGET(button), FALSE);
    SET_INT_DATA(button, PREVIEW_KEY, TRUE);
  }

  LiVESSpinButton *spin = lives_widget_object_get_data(LIVES_WIDGET_OBJECT(button), "spin");
  lives_font_chooser_set_font(fontbutton, capable->def_fontstring);
  lives_spin_button_set_value(spin, capable->font_size);
}


static void font_scale_changed(LiVESRange * scale, LiVESSpinButton * spin) {
  lives_spin_button_set_value(spin, lives_range_get_value(scale));
}

static void font_spin_changed(LiVESRange * scale, LiVESSpinButton * spin) {
  lives_range_set_value(scale, lives_spin_button_get_value(spin));
}


static void font_set_cb(LiVESFontButton * button, LiVESSpinButton * spin) {
  char *fname = lives_font_chooser_get_font(LIVES_FONT_CHOOSER(button));
  LingoFontDesc *lfd = lives_font_chooser_get_font_desc(LIVES_FONT_CHOOSER(button));
  int size = lingo_fontdesc_get_size(lfd);
  if (lingo_fontdesc_size_scaled(lfd)) size /= LINGO_SCALE;

  lives_signal_handler_block(spin, prefsw->font_size_func);
  lives_spin_button_set_value(spin, size / LINGO_SCALE);
  lives_signal_handler_unblock(spin, prefsw->font_size_func);

  lives_free(fname);
  lingo_fontdesc_free(lfd);
  lives_widget_set_sensitive(prefsw->font_pre_button, TRUE);
}

static void font_size_cb(LiVESSpinButton * button, LiVESFontChooser * fchoose) {
  int sval = lives_spin_button_get_value_as_int(button);
  LingoFontDesc *lfd = lives_font_chooser_get_font_desc(fchoose);
  lingo_fontdesc_set_size(lfd, sval * LINGO_SCALE);
  lives_font_chooser_set_font_desc(fchoose, lfd);
  lingo_fontdesc_free(lfd);
  lives_widget_set_sensitive(prefsw->font_pre_button, TRUE);
}
#endif

static boolean check_txtsize(LiVESWidget * combo) {
  LiVESList *list = get_textsizes_list();
  const char *msgtextsize = lives_combo_get_active_text(LIVES_COMBO(combo));
  int idx = lives_list_strcmp_index(list, (livesconstpointer)msgtextsize, TRUE);
  lives_list_free_all(&list);

  if (idx > mainw->max_textsize) {
    show_warn_image(combo, _("Text size may be too large for the screen size"));
    return TRUE;
  }
  hide_warn_image(combo);
  return FALSE;
}

#ifdef ENABLE_JACK
static void copy_entry_text(LiVESEntry * e1, LiVESEntry * e2) {
  if (lives_toggle_button_get_active(prefsw->jack_srv_dup))
    lives_entry_set_text(e2, lives_entry_get_text(e1));
}

static void jack_make_perm(LiVESWidget * widget, livespointer data) {
  future_prefs->jack_opts &= ~(JACK_INFO_TEMP_OPTS | JACK_INFO_TEMP_NAMES);
  apply_button_set_enabled(NULL, NULL);
  lives_widget_set_sensitive(widget, FALSE);
  hide_warn_image(prefsw->jack_acname);
  hide_warn_image(prefsw->jack_tcname);
}

#endif


static boolean _full_reset(lives_obj_t *obj, void *pconfdir) {
  char *confdir = *(char **)pconfdir;
  char *config_file = lives_build_filename(confdir, LIVES_DEF_CONFIG_FILE, NULL);
  lives_rmdir(confdir, TRUE);
  lives_rmdir(prefs->config_datadir, TRUE);
  lives_mkdir_with_parents(confdir, capable->umask);
  lives_free(confdir);
  lives_touch(config_file);
  lives_free(config_file);
  return FALSE;
}


static void do_full_reset(LiVESWidget * widget, livespointer data) {
  // TODO - save set
  char *tmp, *tmp2;
  char *config_dir = lives_build_path(capable->home_dir, LIVES_DEF_CONFIG_DIR, NULL);
  widget_opts.use_markup = TRUE;
  boolean ret = do_yesno_dialogf_with_countdown(2, TRUE, _("<big><b>LiVES will remove all settings and customizations</b></big>\n"
                "\ncontained in %s\nand %s\n\n"
                "Upon restart, you will be guided through the Setup process once more\n\n\n"
                "Click 2 times on the 'Yes' button to confirm that this is waht you want\n"
                "or click 'No' to cancel"),
                (tmp = lives_markup_escape_text(config_dir, -1)),
                (tmp2 = lives_markup_escape_text(prefs->config_datadir, -1)));
  widget_opts.use_markup = FALSE;
  lives_free(tmp); lives_free(tmp2);
  if (!ret) {
    lives_free(config_dir);
    return;
  }
  lives_hook_append(mainw->global_hook_stacks, DESTRUCTION_HOOK, 0, _full_reset, (void *)&config_dir);
  lives_exit(0);
}


#define PANED_MIN 20

static void callibrate_paned(LiVESPaned * p, LiVESWidget * w) {
  int pos;
  //lives_widget_show_now(w);
  lives_widget_context_update();
  pos = lives_paned_get_position(p);
  while (!gtk_widget_get_mapped(w)) {
    //while (!gtk_widget_get_visible(w)) {
    if (pos > PANED_MIN) lives_paned_set_position(p, --pos);
    lives_nanosleep(LIVES_SHORT_SLEEP);
    //lives_widget_show_now(w);
    //lives_widget_queue_draw_and_update(lives_widget_get_toplevel(w));
    lives_widget_context_update();
  }
  lives_paned_set_position(p, ++pos);
  if (gtk_widget_get_mapped(w)) {
    while (gtk_widget_get_mapped(w)) {
      lives_paned_set_position(p, ++pos);
      lives_nanosleep(LIVES_SHORT_SLEEP);
      lives_widget_context_update();
    }
  }
  lives_paned_set_position(p, pos + widget_opts.border_width);
}


static void show_cmdlinehelp(LiVESWidget * w, livespointer data) {
  LiVESTextBuffer *textbuf = lives_text_buffer_new();
  char *title = _("Commandline Options");
  text_window *hlpwin = create_text_window(title, NULL, textbuf, FALSE);
  lives_free(title);

  lives_dialog_add_button_from_stock(LIVES_DIALOG(textwindow->dialog),
                                     LIVES_STOCK_CLOSE, _("_Close Window"), LIVES_RESPONSE_CANCEL);

  print_opthelp(textbuf, NULL, NULL);
  lives_dialog_run(LIVES_DIALOG(hlpwin->dialog));
  lives_widget_destroy(hlpwin->dialog);
}


#ifdef TPLAYWINDOW
static void show_tplay_opts(LiVESButton * button, livespointer data) {
  LiVESWidget *dialog = lives_standard_dialog_new(_("Trickplay Options"),
                        TRUE, DEF_DIALOG_WIDTH, DEF_DIALOG_HEIGHT);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *label, *layout, *hbox, *scrolledwindow;

  char *tmp = lives_big_and_bold(_("Trickplay Options"));

  LiVESResponseType ret;

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(tmp);
  widget_opts.use_markup = FALSE;
  lives_free(tmp);

  lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);

  layout = lives_layout_new(NULL);

  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, layout);

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrolledwindow);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  /* set_pref_widget(SELF_TRANS, */
  /* 		  lives_standard_check_button_new(_("Allow clips to transition with themselves"), */
  /* 						  prefs->tr_self, LIVES_BOX(hbox), */
  /* 						  (tmp = H_("Enabling this allows clips to blend with themselves " */
  /* 							    "during realtime playback.\nIf unset, enabling a " */
  /* 							    "transition filter will have no effect until a " */
  /* 							    "new background clip is selected.")))); */
  /* lives_free(tmp); */

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  lives_standard_check_button_new(_("Transition between foreground clips"),
                                  prefs->autotrans_key > 0, LIVES_BOX(hbox),
                                  (tmp = H_("During realtime playback, pressing shift-page-up and shift-page-down "
                                         "can perform a smooth transition between clips\n")));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_atrans_key = lives_standard_spin_button_new
                                  (_("Autotransition key (experimental)"), prefs->autotrans_key, 0.,
                                   prefs->rte_keys_virtual, 1., 1., 0, LIVES_BOX(hbox),
                                   (tmp = H_("This value defines which effect key is used for the transition\n"
                                          "Transitions may be bound to the selected key in the fx mapping window\n"
                                          "(A setting of zero disables this feature.)")));

  lives_free(tmp);
  ACTIVE(spinbutton_atrans_key, VALUE_CHANGED);

  // TODO - add atrans time
  // - cache frames when loop locked
  // scratch vals
  // faster / slower vals
  // fx param +- vals
  // randomise pb posn when switching clips


  ret = lives_dialog_run(LIVES_DIALOG(dialog));
  lives_widget_destroy(dialog);
}
#endif

/*
  Function creates preferences dialog
*/
_prefsw *create_prefs_dialog(LiVESWidget * saved_dialog) {
  LiVESWidget *dialog_vbox_main;
  LiVESWidget *dialog_table;

  LiVESPixbuf *pixbuf_multitrack;
  LiVESPixbuf *pixbuf_reset;
  LiVESPixbuf *pixbuf_gui;
  LiVESPixbuf *pixbuf_decoding;
  LiVESPixbuf *pixbuf_playback;
  LiVESPixbuf *pixbuf_recording;
  LiVESPixbuf *pixbuf_encoding;
  LiVESPixbuf *pixbuf_effects;
  LiVESPixbuf *pixbuf_directories;
  LiVESPixbuf *pixbuf_warnings;
  LiVESPixbuf *pixbuf_misc;
  LiVESPixbuf *pixbuf_themes;
  LiVESPixbuf *pixbuf_net;
  LiVESPixbuf *pixbuf_jack;
  LiVESPixbuf *pixbuf_midi;

  LiVESWidget *ins_resample;
  LiVESWidget *hbox;
#ifdef ENABLE_JACK
  LiVESWidget *widget;
#endif

  LiVESWidget *layout, *layout2;
#ifdef TPLAYWINDOW
  LiVESWidget *image;
#endif

  LiVESWidget *hbox1;
  LiVESWidget *vbox;

  LiVESWidget *dirbutton, *cbut;

  LiVESWidget *pp_combo;
  LiVESWidget *png;
  LiVESWidget *frame;
  LiVESWidget *mt_enter_defs;

  LiVESWidget *advbutton;
  LiVESWidget *cmdhelp;
#if GTK_CHECK_VERSION(3, 2, 0)
  LiVESWidget *scale;
#endif

  LiVESWidget *sp_red, *sp_green, *sp_blue;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifdef ALSA_MIDI
  LiVESWidget *raw_midi_button;
#endif
#endif
#endif

  LiVESWidget *label;

  // radio button groups
#ifdef ENABLE_JACK
  LiVESSList *rb_group = NULL;
#endif
  LiVESSList *jpeg_png = NULL;
  LiVESSList *mt_enter_prompt = NULL;
  LiVESSList *rb_group2 = NULL;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
#ifdef ALSA_MIDI
  LiVESSList *alsa_midi_group = NULL;
#endif
  LiVESList *mchanlist = NULL;
#endif
#endif
  LiVESSList *autoback_group = NULL;
  LiVESSList *st_interface_group = NULL;

  LiVESSList *asrc_group = NULL;

  // drop down lists
  LiVESList *themes = NULL;
  LiVESList *ofmt = NULL;
  LiVESList *ofmt_all = NULL;
  LiVESList *audp = NULL;
  LiVESList *encoders = NULL;
  LiVESList *vid_playback_plugins = NULL;
  LiVESList *textsizes_list;
  LiVESList *rmodelist = NULL;
  LiVESList *radjlist = NULL;

  lives_colRGBA64_t rgba;

  char **array, **filt;
  char *tmp, *tmp2, *tmp3;
  char *theme, *msg;

#ifdef ENABLE_OSC
#ifdef OMC_MIDI_IMPL
  char *midichan;
#endif
#endif

  boolean pfsm;
  boolean has_ap_rec = FALSE;
  boolean sel_last = FALSE;

#ifdef ENABLE_JACK
  boolean ajack_cfg_exists = FALSE;
  boolean tjack_cfg_exists = FALSE;
#endif

  int woph = widget_opts.packing_height;
  int wopw = widget_opts.packing_width;
  int i;

  // Allocate memory for the preferences structure
  _prefsw *prefsw = (_prefsw *)(lives_malloc(sizeof(_prefsw)));
  prefsw->right_shown = NULL;
  mainw->prefs_need_restart = FALSE;

  prefsw->accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  if (!saved_dialog) {
    // Create new modal dialog window and set some attributes
    int wobw = widget_opts.border_width;
    widget_opts.border_width = 0;
    prefsw->prefs_dialog = lives_standard_dialog_new(_("Preferences"), FALSE, PREFWIN_WIDTH, PREFWIN_HEIGHT);
    widget_opts.border_width = wobw;
    lives_window_add_accel_group(LIVES_WINDOW(prefsw->prefs_dialog), prefsw->accel_group);
    lives_window_set_default_size(LIVES_WINDOW(prefsw->prefs_dialog), PREFWIN_WIDTH, PREFWIN_HEIGHT);
  } else prefsw->prefs_dialog = saved_dialog;

  prefsw->ignore_apply = FALSE;
  //prefs->cb_is_switch = TRUE; // TODO: intercept TOGGLED handler

  // Get dialog's vbox and show it
  dialog_vbox_main = lives_dialog_get_content_area(LIVES_DIALOG(prefsw->prefs_dialog));

  // Create dialog horizontal panels
  prefsw->dialog_hpaned = lives_hpaned_new();
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->dialog_hpaned), widget_opts.border_width * 2);

  // Create dialog table for the right panel controls placement
  dialog_table = lives_vbox_new(FALSE, 0);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(dialog_table), hbox, FALSE, FALSE, widget_opts.packing_height);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  prefsw->tlabel = lives_standard_label_new(NULL);
  lives_widget_apply_theme2(prefsw->tlabel, LIVES_WIDGET_STATE_NORMAL, TRUE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_box_pack_start(LIVES_BOX(hbox), prefsw->tlabel, TRUE, TRUE, 0);

#if GTK_CHECK_VERSION(3, 16, 0)
  if (mainw->pretty_colours) {
    char *colref2 = gdk_rgba_to_string(&palette->menu_and_bars);
    char *colref = gdk_rgba_to_string(&palette->normal_back);
    char *tmp = lives_strdup_printf("linear-gradient(%s, %s)", colref2, colref);
    set_css_value_direct(prefsw->tlabel, LIVES_WIDGET_STATE_NORMAL, "",
                         "background-image", tmp);
    lives_free(colref); lives_free(colref2);
    lives_free(tmp);
    set_css_value_direct(LIVES_WIDGET(prefsw->tlabel), LIVES_WIDGET_STATE_NORMAL, "", "border-top-left-radius", "20px");
    set_css_value_direct(LIVES_WIDGET(prefsw->tlabel), LIVES_WIDGET_STATE_NORMAL, "", "border-top-right-radius", "20px");
  }
#endif

  lives_widget_show_all(dialog_table);
  lives_widget_process_updates(dialog_table);
  lives_widget_set_no_show_all(dialog_table, TRUE);

  // Create preferences list with invisible headers
  prefsw->prefs_list = lives_tree_view_new();

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme(prefsw->prefs_list, LIVES_WIDGET_STATE_SELECTED);
  }

  lives_tree_view_set_headers_visible(LIVES_TREE_VIEW(prefsw->prefs_list), FALSE);

  // Place panels into main vbox
  lives_box_pack_start(LIVES_BOX(dialog_vbox_main), prefsw->dialog_hpaned, TRUE, TRUE, 0);

  // Place list on the left panel
  pref_init_list(prefsw->prefs_list);

  prefsw->list_scroll =
    lives_scrolled_window_new_with_adj
    (lives_tree_view_get_hadjustment(LIVES_TREE_VIEW(prefsw->prefs_list)), NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(prefsw->list_scroll), LIVES_POLICY_AUTOMATIC,
                                   LIVES_POLICY_AUTOMATIC);
  lives_container_add(LIVES_CONTAINER(prefsw->list_scroll), prefsw->prefs_list);

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme3(prefsw->prefs_list, LIVES_WIDGET_STATE_NORMAL);
  }

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme(dialog_table, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(prefsw->dialog_hpaned, LIVES_WIDGET_STATE_NORMAL);
  }

  lives_paned_pack(1, LIVES_PANED(prefsw->dialog_hpaned), prefsw->list_scroll, TRUE, FALSE);
  // Place table on the right panel

  lives_paned_pack(2, LIVES_PANED(prefsw->dialog_hpaned), dialog_table, TRUE, FALSE);

#if GTK_CHECK_VERSION(3, 0, 0)
  lives_paned_set_position(LIVES_PANED(prefsw->dialog_hpaned), PREFS_PANED_POS);
#else
  lives_paned_set_position(LIVES_PANED(prefsw->dialog_hpaned), PREFS_PANED_POS / 2);
#endif

  prefsw->vbox_right_reset = lives_vbox_new(FALSE, 0);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_reset));
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  msg = _("LiVES version details:");
  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);

  msg = lives_strdup_printf(_("LiVES version %s powered by Weed ABI version %d, Weed Filter API version %d,\n"
                              "RFX version %s, Clip Header Version %d"),
                            LiVES_VERSION, WEED_ABI_VERSION, WEED_FILTER_API_VERSION, RFX_VERSION, LIVES_CLIP_HEADER_VERSION);

  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  msg = _("Location of LiVES executable:");
  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);

  msg = lives_strdup_printf(_("%s\n"), capable->myname_full);
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);

  msg = _("Current process ID:");
  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);

  msg = lives_strdup_printf(_("%d"), capable->mainpid);
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  lives_free(msg);
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_reset), hbox, TRUE, FALSE, widget_opts.packing_height >> 1);
  prefsw->cmdline_entry = lives_standard_entry_new(_("Default startup arguments"),
                          prefs->cmdline_args ? prefs->cmdline_args : NULL, LONGEST_ENTRY_WIDTH, PATH_MAX * 2,
                          LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_reset), hbox, TRUE, FALSE, widget_opts.packing_height >> 1);
  msg = lives_strdup_printf(_("Default commandline arguments will be read from the first line of the file %s\n"
                              "If that file does not exist, then system defaults will be read from %s "
                              "(if that file exists)\nArguments entered on the commandline will be added "
                              "AFTER any default arguments\n\nExample: -jackopts 16 -jackserver myserver\n"),
                            capable->extracmds_file[0], capable->extracmds_file[1]);

  label = lives_standard_label_new(msg);
  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, FALSE, widget_opts.packing_width);
  lives_free(msg);

  cmdhelp = lives_standard_button_new_from_stock_full
            (LIVES_LIVES_STOCK_HELP_INFO, _("Commandline reference (opens in new window)"),
             -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(cmdhelp), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(show_cmdlinehelp), NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_reset));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_reset));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Reset"), FALSE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->the_button = lives_standard_button_new_from_stock_full(LIVES_STOCK_REFRESH,
                       _("\t\tCLICK HERE TO COMPLETELY RESET LiVES \t (confirmation required)\t\t"),
                       -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->the_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(do_full_reset), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  pixbuf_reset = lives_pixbuf_new_from_stock_at_size(LIVES_STOCK_REFRESH, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_reset, _("Startup / Reset"), LIST_ENTRY_RESET);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->vbox_right_reset);

  // -------------------,
  // gui controls       |
  // -------------------'
  prefsw->vbox_right_gui = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_gui = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_gui);
  prefsw->right_shown = prefsw->vbox_right_gui;

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  tmp = lives_big_and_bold(_("Default interface font"));
  widget_opts.use_markup = TRUE;
  lives_layout_add_label(LIVES_LAYOUT(layout), tmp, TRUE);
  lives_free(tmp);
  widget_opts.use_markup = FALSE;

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

#if GTK_CHECK_VERSION(3, 2, 0)
  prefsw->font_pre_button =
    lives_standard_button_new_from_stock_full(LIVES_STOCK_APPLY, NULL, -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->fontbutton = lives_standard_font_chooser_new(capable->font_name);
  lives_standard_font_chooser_set_size(LIVES_FONT_CHOOSER(prefsw->fontbutton), capable->font_size);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->font_pre_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(font_preview_clicked), prefsw->fontbutton);

  lives_layout_pack(LIVES_BOX(hbox), prefsw->fontbutton);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->font_size_spin = lives_standard_spin_button_new(_("Size"), capable->font_size,
                           4., 128., 1., 1, 0, LIVES_BOX(hbox), NULL);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->font_pre_button), "spin",
                               prefsw->font_size_spin);
  font_preview_clicked(LIVES_BUTTON(prefsw->font_pre_button), LIVES_FONT_CHOOSER(prefsw->fontbutton));

  ACTIVE(fontbutton, FONT_SET);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->fontbutton), LIVES_WIDGET_FONT_SET_SIGNAL,
                            LIVES_GUI_CALLBACK(font_set_cb), prefsw->font_size_spin);

  ACTIVE(font_size_spin, VALUE_CHANGED);

  prefsw->font_size_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->font_size_spin),
                           LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(font_size_cb),
                           (livespointer)prefsw->fontbutton);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  scale = lives_standard_hscale_new(NULL);
  lives_range_set_range(LIVES_RANGE(scale), 6., 24.);
  lives_range_set_value(LIVES_RANGE(scale), capable->font_size);
  label = lives_standard_label_new("6px");
  lives_layout_pack(LIVES_BOX(hbox), label);
  lives_layout_pack(LIVES_BOX(hbox), scale);
  label = lives_standard_label_new("24px");
  lives_layout_pack(LIVES_BOX(hbox), label);
  lives_widget_set_size_request(scale, DEF_SLIDER_WIDTH, -1);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(scale), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(font_scale_changed), prefsw->font_size_spin);

  lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(prefsw->font_size_spin), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(font_spin_changed), scale);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
#endif

  prefsw->fs_max_check =
    lives_standard_check_button_new(_("Open file selection maximised"), prefs->fileselmax, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->recent_check =
    lives_standard_check_button_new(_("Show recent files in the File menu"), prefs->show_recent, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->stop_screensaver_check =
    lives_standard_check_button_new(_("Stop screensaver on playback    "), prefs->stop_screensaver, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->open_maximised_check = lives_standard_check_button_new(_("Open main window maximised"), prefs->open_maximised,
                                 LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  SET_PREF_WIDGET(SHOW_TOOLBAR, lives_standard_check_button_new(_("Show toolbar when background is blanked"),
                  prefs->show_tool, LIVES_BOX(hbox), NULL));

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->mouse_scroll =
    lives_standard_check_button_new(_("Allow mouse wheel to switch clips"), prefs->mouse_scroll_clips, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_ce_maxspect =
    lives_standard_check_button_new(_("Shrink previews to fit in interface"), prefs->ce_maxspect, LIVES_BOX(hbox),
                                    (tmp = H_("Setting is assumed automatically if letterbox playback is enabled")));
  lives_free(tmp);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_hfbwnp =
    lives_standard_check_button_new(_("Hide framebar when not playing"), prefs->hfbwnp, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

#if GTK_CHECK_VERSION(2, 12, 0)
  prefsw->checkbutton_show_ttips =
    lives_standard_check_button_new(_("Show tooltips"), prefs->show_tooltips, LIVES_BOX(hbox), NULL);
#else
  prefsw->checkbutton_show_ttips = NULL;
#endif

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_show_asrc =
    lives_standard_check_button_new(_("Show audio source in toolbar"), prefs->show_asrc, LIVES_BOX(hbox), NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), hbox, FALSE, FALSE, widget_opts.packing_height);
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  label = lives_standard_label_new(_("Startup mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rb_startup_ce = lives_standard_radio_button_new(_("_Clip editor"), &st_interface_group, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rb_startup_mt = lives_standard_radio_button_new(_("_Multitrack mode"), &st_interface_group, LIVES_BOX(hbox), NULL);

  if (prefs->vj_mode)
    show_warn_image(prefsw->rb_startup_mt, _("Disabled in VJ mode"));

  if (future_prefs->startup_interface == STARTUP_MT) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_mt), TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rb_startup_ce), TRUE);
  }

  add_fill_to_box(LIVES_BOX(hbox));

  //
  // multihead support (inside Gui part)
  //

  pfsm = prefs->force_single_monitor;
  prefs->force_single_monitor = FALSE;
  get_monitors(FALSE);
  nmons = capable->nmonitors;

  label = lives_standard_label_new(_("Multi-head support"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_gui), label, FALSE, FALSE, widget_opts.packing_height);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_gmoni = lives_standard_spin_button_new(_("Monitor number for LiVES interface"), prefs->gui_monitor, 1, nmons,
                             1., 1., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_pmoni =
    lives_standard_spin_button_new(_("Monitor number for playback"),
                                   prefs->play_monitor, 0,
                                   nmons == 1 ? 0 : nmons, 1., 1., 0, LIVES_BOX(hbox),
                                   (tmp = lives_strdup(H_("A setting of 0 means use all available "
                                          "monitors (only works with some playback "
                                          "plugins)."))));
  lives_free(tmp);
  prefs->force_single_monitor = pfsm;

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->forcesmon_hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->forcesmon = lives_standard_check_button_new((tmp = (_("Force single monitor"))),
                      prefs->force_single_monitor, LIVES_BOX(prefsw->forcesmon_hbox),
                      (tmp2 = (_("Ignore all except the first monitor."))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_widget_set_no_show_all(prefsw->forcesmon, TRUE);

  if (nmons <= 1) {
    lives_widget_set_sensitive(prefsw->spinbutton_gmoni, FALSE);
    lives_widget_set_sensitive(prefsw->spinbutton_pmoni, FALSE);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->forcesmon), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_forcesmon_toggled), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->ce_thumbs = lives_standard_check_button_new(_("Show clip thumbnails during playback"), prefs->ce_thumb_mode,
                      LIVES_BOX(hbox), NULL);
  lives_widget_set_no_show_all(prefsw->ce_thumbs, TRUE);

  lives_widget_set_sensitive(prefsw->ce_thumbs, prefs->play_monitor != prefs->gui_monitor &&
                             prefs->play_monitor != 0 && !prefs->force_single_monitor &&
                             capable->nmonitors > 1);

  pmoni_gmoni_changed(NULL, (livespointer)prefsw);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_gui));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_gui));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  // workaround for idiosyncracy of lives_layout
  widget_opts.pack_end = TRUE;
  prefsw->msgs_unlimited = lives_standard_check_button_new(_("_Unlimited"),
                           prefs->max_messages < 0, LIVES_BOX(hbox), NULL);
  widget_opts.pack_end = FALSE;

  ACTIVE(msgs_unlimited, TOGGLED);

  prefsw->nmessages_spin = lives_standard_spin_button_new(_("Number of _Info Messages to Buffer"),
                           ABS(prefs->max_messages), 0., 100000., 1., 1., 0,
                           LIVES_BOX(hbox), NULL);
  ACTIVE(nmessages_spin, VALUE_CHANGED);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_unlimited), prefsw->nmessages_spin, TRUE);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->nmessages_spin), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(widget_inact_toggle), prefsw->msgs_unlimited);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->msgs_mbdis = lives_standard_check_button_new(_("Show _message area in the interface"),
                       future_prefs->show_msg_area, LIVES_BOX(hbox), NULL);
  ACTIVE(msgs_mbdis, TOGGLED);

  if (!capable->can_show_msg_area) show_warn_image(prefsw->msgs_mbdis, _("Due to screen size restrictions, "
        "messages cannot currently be shown in the interface"));

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->msgs_nopbdis = lives_standard_check_button_new(_("_Pause message output during playback"),
                         prefs->msgs_nopbdis, LIVES_BOX(hbox), H_("Setting this can improve performance "
                             "of the player engine during playback"));
  ACTIVE(msgs_nopbdis, TOGGLED);

  textsizes_list = get_textsizes_list();

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->msg_textsize_combo = lives_standard_combo_new(_("Message Area _Text Size"), textsizes_list,
                               LIVES_BOX(hbox), NULL);

  lives_combo_set_active_index(LIVES_COMBO(prefsw->msg_textsize_combo), prefs->msg_textsize);

  check_txtsize(prefsw->msg_textsize_combo);
  lives_signal_connect_after(LIVES_WIDGET_OBJECT(prefsw->msg_textsize_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(check_txtsize), NULL);

  lives_list_free_all(&textsizes_list);

  ACTIVE(msg_textsize_combo, CHANGED);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis), prefsw->msgs_nopbdis, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis), prefsw->msgs_unlimited, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis), prefsw->nmessages_spin, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->msgs_mbdis), prefsw->msg_textsize_combo, FALSE);

  pixbuf_gui = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_GUI, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_gui, _("GUI"), LIST_ENTRY_GUI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_gui);

  // -----------------------,
  // multitrack controls    |
  // -----------------------'

  prefsw->vbox_right_multitrack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_multitrack), widget_opts.border_width * 2);

  prefsw->scrollw_right_multitrack = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_multitrack);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When entering Multitrack mode:"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  add_fill_to_box(LIVES_BOX(hbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->mt_enter_prompt = lives_standard_radio_button_new(_("_Prompt me for width, height, fps and audio settings"),
                            &mt_enter_prompt, LIVES_BOX(hbox), NULL);

  mt_enter_defs = lives_standard_radio_button_new(_("_Always use the following values:"),
                  &mt_enter_prompt, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mt_enter_defs), !prefs->mt_enter_prompt);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, 0);

  prefsw->checkbutton_render_prompt = lives_standard_check_button_new(_("Use these same _values for rendering a new clip"),
                                      !prefs->render_prompt, LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(mt_enter_defs), prefsw->checkbutton_render_prompt, FALSE);

  frame = add_video_options(&prefsw->spinbutton_mt_def_width, !mainw->multitrack ? prefs->mt_def_width : cfile->hsize,
                            &prefsw->spinbutton_mt_def_height,
                            !mainw->multitrack ? prefs->mt_def_height : cfile->vsize, &prefsw->spinbutton_mt_def_fps,
                            !mainw->multitrack ? prefs->mt_def_fps : cfile->fps, NULL, 0, NULL, NULL);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), frame, FALSE, FALSE, widget_opts.packing_height);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(mt_enter_defs), frame, FALSE);

  hbox = add_audio_options(&prefsw->backaudio_checkbutton, &prefsw->pertrack_checkbutton);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->backaudio_checkbutton), prefs->mt_backaudio > 0);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->pertrack_checkbutton), prefs->mt_pertrack_audio);

  // must be done after creating check buttons
  resaudw = create_resaudw(4, NULL, prefsw->vbox_right_multitrack);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(mt_enter_defs), resaudw->frame, FALSE);

  // must be done after resaudw
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_multitrack), hbox, FALSE, FALSE, widget_opts.packing_height);

  lives_widget_set_sensitive(prefsw->backaudio_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  lives_widget_set_sensitive(prefsw->pertrack_checkbutton,
                             lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton)));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(mt_enter_defs), hbox, FALSE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_multitrack));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_multitrack));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_mt_undo_buf = lives_standard_spin_button_new(_("_Undo buffer size (MB)"),
                                   prefs->mt_undo_buf, 0., ONE_MILLION, 1., 1., 0,
                                   LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_mt_exit_render = lives_standard_check_button_new(_("_Exit multitrack mode after rendering"),
                                       prefs->mt_exit_render, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_("Auto backup layouts:"));
  lives_box_pack_end(LIVES_BOX(hbox), label, FALSE, TRUE, widget_opts.packing_width * 2);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->mt_autoback_every = lives_standard_radio_button_new(_("_Every"), &autoback_group, LIVES_BOX(hbox), NULL);

  widget_opts.packing_width >>= 1;
  widget_opts.swap_label = TRUE;
  prefsw->spinbutton_mt_ab_time = lives_standard_spin_button_new(_("seconds"), 120., 10., 1800., 1., 10., 0, LIVES_BOX(hbox),
                                  NULL);
  widget_opts.swap_label = FALSE;
  widget_opts.packing_width = wopw;

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->mt_autoback_always = lives_standard_radio_button_new(_("After every _change"),
                               &autoback_group, LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->mt_autoback_never = lives_standard_radio_button_new(_("_Never"), &autoback_group, LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->mt_autoback_every), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_mtbackevery_toggled), prefsw);

  if (prefs->mt_auto_back == 0) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_always), TRUE);
  } else if (prefs->mt_auto_back == -1) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_never), TRUE);
  } else {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->mt_autoback_every), TRUE);
    lives_spin_button_set_value(LIVES_SPIN_BUTTON(prefsw->spinbutton_mt_ab_time), prefs->mt_auto_back);
  }

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_max_disp_vtracks = lives_standard_spin_button_new(_("Maximum number of visible tracks"),
                                        prefs->max_disp_vtracks, 5., 15.,
                                        1., 1., 0, LIVES_BOX(hbox), NULL);

  pixbuf_multitrack = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MULTITRACK, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_multitrack, _("Multitrack/Render"), LIST_ENTRY_MULTITRACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_multitrack);

  // ---------------,
  // decoding       |
  // ---------------'

  prefsw->vbox_right_decoding = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_decoding), widget_opts.border_width * 2);

  prefsw->scrollw_right_decoding = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_decoding);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_instant_open =
    lives_standard_check_button_new((tmp = (_("Use instant opening when possible"))),
                                    prefs->instant_open, LIVES_BOX(hbox),
                                    (tmp2 = (H_("Enable instant opening of some file types using decoder plugins"))));

  lives_free(tmp); lives_free(tmp2);

  // advanced instant opening
  advbutton = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES, _("_Advanced"),
              DEF_BUTTON_WIDTH, -1, LIVES_BOX(hbox), TRUE, NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_decplug_advanced_clicked), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_instant_open), advbutton, FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->video_open_entry = lives_standard_entry_new(_("Video open command (fallback)"),
                             prefs->video_open_command, -1, PATH_MAX * 2,
                             LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new_with_tooltips(_("Fallback image format"), LIVES_BOX(hbox),
          _("The image format to be used when opening clips\n"
            "for which there is no instant decoder candidate."));

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->jpeg = lives_standard_radio_button_new(_("_jpeg"), &jpeg_png, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  png = lives_standard_radio_button_new(_("_png"), &jpeg_png, LIVES_BOX(hbox), NULL);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(png), !strcmp(prefs->image_ext, LIVES_FILE_EXT_PNG));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

  label = lives_standard_label_new(_("(Check Help/Troubleshoot to see which image formats are supported)"));
  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, widget_opts.packing_width);

  if (prefs->ocp == -1) prefs->ocp = get_int_pref(PREF_OPEN_COMPRESSION_PERCENT);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("Image compression"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  widget_opts.swap_label = TRUE;
  widget_opts.expand = LIVES_EXPAND_DEFAULT_HEIGHT;
  prefsw->spinbutton_ocp =
    lives_standard_spin_button_new(("  %  "), prefs->ocp, 0., 60., 1., 5., 0, LIVES_BOX(hbox),
                                   (tmp = H_("A higher value will require less disk space\n"
                                          "but may slow down the application.\n"
                                          "For jpeg, a higher value may lead to\n"
                                          "loss of image quality\n"
                                          "The default value of 15 is recommended")));
  lives_free(tmp);
  widget_opts.swap_label = FALSE;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_auto_deint = lives_standard_check_button_new((tmp =
                                     _("Enable automatic deinterlacing when possible")),
                                   prefs->auto_deint, LIVES_BOX(hbox),
                                   (tmp2 = (H_("Automatically deinterlace frames when a decoder plugin suggests it"))));
  lives_free(tmp); lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_auto_trim = lives_standard_check_button_new((tmp =
                                    _("Automatic trimming / padding of audio when possible")),
                                  prefs->auto_trim_audio, LIVES_BOX(hbox),
                                  (tmp2 = (H_("Automatically trim or pad audio to try to keep it in synch with video.\n"
                                          "This operation can be reversed after loading the clip, if so desired."))));
  lives_free(tmp); lives_free(tmp2);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_decoding));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_decoding), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_concat_images
    = lives_standard_check_button_new((tmp = _("When opening multiple files, concatenate images into one clip")),
                                      prefs->concat_images, LIVES_BOX(hbox),
                                      (tmp2 = H_("Choose whether multiple images are opened as separate clips or "
                                          "combined into a single clip.\n")));

  pixbuf_decoding = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_DECODING, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_decoding, _("Decoding"), LIST_ENTRY_DECODING);

  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_decoding);

  // ---------------,
  // playback       |
  // ---------------'

  prefsw->vbox_right_playback = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_playback = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_playback);

  frame = lives_standard_frame_new(_("VIDEO"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, FALSE, FALSE, 0);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->pbq_adaptive =
    lives_standard_check_button_new(
      (tmp = lives_strdup_printf(_("_Enable adaptive quality (%s)"),
                                 mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED])),
      prefs->pbq_adaptive, LIVES_BOX(hbox),
      H_("If enabled, quality will be continuously adjusted during playback\n"
         "in order to maintain a smooth frame rate"));
  lives_free(tmp);

  prefsw->pbq_list = NULL;
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("Low - can improve performance on slower machines")));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("Normal - recommended for most users")));
  // TRANSLATORS: video quality, max len 50
  prefsw->pbq_list = lives_list_append(prefsw->pbq_list, (_("High - can improve quality on very fast machines")));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->pbq_combo = lives_standard_combo_new((tmp = (_("Preview _quality"))), prefsw->pbq_list, LIVES_BOX(hbox),
                      H_("The preview quality for video playback, ignored if adaptive quality is selected"));

  lives_free(tmp);

  switch (future_prefs->pb_quality) {
  case PB_QUALITY_HIGH:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 2);
    break;
  case PB_QUALITY_MED:
    lives_combo_set_active_index(LIVES_COMBO(prefsw->pbq_combo), 1);
  }

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->pbq_adaptive), prefsw->pbq_combo, TRUE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_show_stats =
    lives_standard_check_button_new(_("_Show FPS statistics"),
                                    prefs->show_player_stats,
                                    LIVES_BOX(hbox), H_("Print a message detailing the mean FPS rate after playback ends\n"
                                        "Can be useful for benchmarking"));
#ifdef TPLAYWINDOW
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->tplay_butt = lives_standard_button_new_from_stock_full
                       (NULL, _("_Trickplay Options(Advanced)"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);
  image = lives_image_find_in_stock(LIVES_ICON_SIZE_LARGE_TOOLBAR, "trick", NULL);
  lives_standard_button_set_image(LIVES_BUTTON(prefsw->tplay_butt), image, TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->tplay_butt), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(show_tplay_opts), NULL);
#endif
  add_hsep_to_box(LIVES_BOX(vbox));

  layout = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  lives_layout_add_row(LIVES_LAYOUT(layout));
  hbox = lives_layout_expansion_row_new(LIVES_LAYOUT(layout), NULL);

  SET_PREF_WIDGET(PB_HIDE_GUI,
                  lives_standard_check_button_new
                  (_("HIDE MAIN INTERFACE WHEN PLAYNIG IN SEPARATE WINDOW"),
                   prefs->pb_hide_gui, LIVES_BOX(hbox), (tmp =
                         H_("Checking this will cause the main interface window to become hidden\n"
                            "whenever LiVES is playing in the separate playback window\n"
                            "Ending playback or switching back to embedded player mode will cause the GUI\n"
                            "to be reshown Only works in Clip Editor mode."))));
  lives_free(tmp);

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Use letterboxing by default:"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_lb = lives_standard_check_button_new(_("In Clip Edit Mode"),
                           prefs->letterbox, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_lbmt = lives_standard_check_button_new(_("In Multitrack Mode"),
                             future_prefs->letterbox_mt, LIVES_BOX(hbox),
                             (tmp = H_("This setting only affects newly created layouts.\n"
                                       "To change the current layout, use menu option\n'Tools' / 'Change Width, Height and Audio Values'\n"
                                       "in the multitrack window")));
  lives_free(tmp);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->no_lb_gens = lives_standard_check_button_new(_("Avoid letterboxing generators"),
                       prefs->no_lb_gens, LIVES_BOX(hbox),
                       (tmp = H_("If this is set, generator plugins will try to dynamically adjust their aspect ratio\n"
                                 "to exactly fill the player or blended clip.\n"
                                 "If unset, generators will always maintain their default aspect ratio,"
                                 "but may leave a gap at the edge of the containing frame.")));

  lives_free(tmp);

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Monitor gamma setting:"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_screengamma = lives_standard_check_button_new(_("Apply screen gamma"),
                                    prefs->use_screen_gamma, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_gamma = lives_standard_spin_button_new(_("Screen gamma value"),
                             prefs->screen_gamma, 1.2, 3.0, .01, .1, 2,
                             LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_screengamma), prefsw->spinbutton_gamma, FALSE);

  add_hsep_to_box(LIVES_BOX(vbox));

  layout = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  if (ARE_UNCHECKED(vid_playback_plugins)) {
    capable->plugins_list[PLUGIN_SUBTYPE_VIDEO_PLAYER] = get_plugin_list(PLUGIN_VID_PLAYBACK, TRUE, NULL, "-" DLL_EXT);
    if (capable->plugins_list[PLUGIN_SUBTYPE_VIDEO_PLAYER]) capable->has_vid_playback_plugins = PRESENT;
    else capable->has_vid_playback_plugins = MISSING;
  }

  vid_playback_plugins = lives_list_copy_strings(capable->plugins_list[PLUGIN_SUBTYPE_VIDEO_PLAYER]);

  vid_playback_plugins = lives_list_prepend(vid_playback_plugins,
                         lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  pp_combo = lives_standard_combo_new(_("_Plugin"), vid_playback_plugins, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  advbutton = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES, _("_Advanced"),
              DEF_BUTTON_WIDTH, -1, LIVES_BOX(hbox), TRUE, NULL);

  THREAD_INTENTION = OBJ_INTENTION_PLAY;
  lives_signal_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vpp_advanced_clicked), NULL);

  if (mainw->vpp) {
    lives_combo_set_active_string(LIVES_COMBO(pp_combo), mainw->vpp->soname);
  } else {
    lives_combo_set_active_index(LIVES_COMBO(pp_combo), 0);
    lives_widget_set_sensitive(advbutton, FALSE);
  }
  lives_list_free_all(&vid_playback_plugins);

  lives_signal_sync_connect_after(LIVES_WIDGET_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_vpp_changed), (livespointer) advbutton);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_stream_audio =
    lives_standard_check_button_new((tmp = (_("Stream audio"))), prefs->stream_audio_out, LIVES_BOX(hbox),
                                    (tmp2 = _("Stream audio to playback plugin")));
  lives_free(tmp); lives_free(tmp2);

  prefsw_set_astream_settings(mainw->vpp, prefsw);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->checkbutton_stream_audio), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(stream_audio_toggled), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_rec_after_pb =
    lives_standard_check_button_new((tmp = (_("Record player output"))),
                                    (prefs->rec_opts & REC_AFTER_PB), LIVES_BOX(hbox),
                                    (tmp2 = lives_strdup
                                        (_("Record output from player instead of input to player"))));
  lives_free(tmp); lives_free(tmp2);

  /// TODO !!!
  lives_widget_set_sensitive(prefsw->checkbutton_rec_after_pb, FALSE);

  prefsw_set_rec_after_settings(mainw->vpp, prefsw);

  //-

  frame = lives_standard_frame_new(_("AUDIO"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_playback), frame, FALSE, FALSE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  audp = lives_list_append(audp, lives_strdup_printf("%s", mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

#ifdef HAVE_PULSE_AUDIO
  audp = lives_list_append(audp, lives_strdup_printf("%s (%s)", AUDIO_PLAYER_PULSE_AUDIO,
                           mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  has_ap_rec = TRUE;
#endif

#ifdef ENABLE_JACK
  if (!has_ap_rec) audp = lives_list_append(audp, lives_strdup_printf("%s (%s)", AUDIO_PLAYER_JACK,
                            mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]));
  else audp = lives_list_append(audp, lives_strdup_printf(AUDIO_PLAYER_JACK));
  has_ap_rec = TRUE;
#endif

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->audp_combo = lives_standard_combo_new(_("_Player"), audp, LIVES_BOX(hbox), NULL);

  has_ap_rec = FALSE;

  prefsw->jack_int_label =
    lives_layout_add_label(LIVES_LAYOUT(layout),
                           _("(See also the Jack Integration tab for jack startup options)"), TRUE);
  lives_widget_set_no_show_all(prefsw->jack_int_label, TRUE);

  prefsw->audp_name = NULL;

  if (prefs->audio_player == AUD_PLAYER_NONE) {
    prefsw->audp_name = lives_strdup_printf("%s", mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);
  }

#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE) {
    prefsw->audp_name = lives_strdup_printf("%s (%s)", AUDIO_PLAYER_PULSE_AUDIO,
                                            mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
  }
  has_ap_rec = TRUE;
#endif

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    if (!has_ap_rec)
      prefsw->audp_name = lives_strdup_printf("%s (%s)", AUDIO_PLAYER_JACK,
                                              mainw->string_constants[LIVES_STRING_CONSTANT_RECOMMENDED]);
    else prefsw->audp_name = lives_strdup_printf(AUDIO_PLAYER_JACK);
  }
  has_ap_rec = TRUE;
#endif

  if (prefsw->audp_name)
    lives_combo_set_active_string(LIVES_COMBO(prefsw->audp_combo), prefsw->audp_name);
  prefsw->orig_audp_name = lives_strdup(prefsw->audp_name);

  //---

#ifdef HAVE_PULSE_AUDIO
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_parestart = lives_standard_check_button_new((tmp = (_("Restart pulseaudio on LiVES startup"))),
                                  prefs->pa_restart, LIVES_BOX(hbox),
                                  (tmp2 = (_("Recommended, but may interfere with other running "
                                          "audio applications"))));
  lives_free(tmp); lives_free(tmp2);

  ACTIVE(checkbutton_parestart, TOGGLED);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  tmp = lives_strdup_printf(_("Pulseaudio restart command: %s -k"), EXEC_PULSEAUDIO);
  prefsw->audio_command_entry = lives_standard_entry_new(tmp, prefs->pa_start_opts, SHORT_ENTRY_WIDTH, PATH_MAX * 2,
                                LIVES_BOX(hbox), NULL);
  ACTIVE(audio_command_entry, CHANGED);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_parestart), prefsw->audio_command_entry, FALSE);

  if (prefs->audio_player != AUD_PLAYER_PULSE) lives_widget_set_sensitive(prefsw->checkbutton_parestart, FALSE);
#endif

  add_hsep_to_box(LIVES_BOX(vbox));

  layout = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_afollow = lives_standard_check_button_new(_("Audio follows video _rate/direction"),
                                (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) ? TRUE : FALSE,
                                LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_aclips = lives_standard_check_button_new(_("Audio follows video _clip switches"),
                               (prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) ? TRUE : FALSE,
                               LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  layout2 = lives_layout_new(LIVES_BOX(hbox));

  lives_layout_add_label(LIVES_LAYOUT(layout2), _("Resync audio when:"), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout2));

  //hbox = widget_opts.last_container;

  prefsw->resync_fps = lives_standard_check_button_new(_("fps is reset"),
                       (prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_FPS) ? FALSE : TRUE,
                       LIVES_BOX(hbox),
                       H_("Determines whether the audio rate and position are\n"
                          "resynchronised with video "
                          "whenever the 'Reset FPS' key is activated.\n"));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout2));
  prefsw->resync_adir = lives_standard_check_button_new(_("audio direction changes"),
                        (prefs->audio_opts & AUDIO_OPTS_RESYNC_ADIR) ? TRUE : FALSE,
                        LIVES_BOX(hbox),
                        H_("Determines whether the audio rate and position are\n"
                           "resynchronised with video "
                           "whenever the audio playback direction changes\n"
                           "as a result of video direction changing"));

  lives_layout_add_row(LIVES_LAYOUT(layout2));
  lives_layout_add_fill(LIVES_LAYOUT(layout2), TRUE);
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout2));

  prefsw->resync_vpos = lives_standard_check_button_new(_("video position 'jumps'"),
                        (prefs->audio_opts & AUDIO_OPTS_NO_RESYNC_VPOS) ? FALSE : TRUE,
                        LIVES_BOX(hbox),
                        H_("Determines whether the audio rate and position are\n"
                           "resynchronised with video "
                           "whenever the video position is changed "
                           "via means other than normal playback\n"
                           "E.g when clicking on the timeline, or when a 'bookmark' is activated"));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout2));
  prefsw->resync_aclip = lives_standard_check_button_new(_("audio clip changes"),
                         (prefs->audio_opts & AUDIO_OPTS_RESYNC_ACLIP) ? TRUE : FALSE,
                         LIVES_BOX(hbox),
                         H_("Determines whether the audio rate and position are\n"
                            "resynchronised with video "
                            "whenever the current audio clip changes"));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("(Audio will only be resynchronised if unlocked "
                         "and both audio and video are playing the identical clip)"), FALSE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Options for Audio Lock (toggle with a / shift + A durign playback)"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->afreeze_lock = lives_standard_check_button_new(_("Freeze button affects locked audio"),
                         (prefs->audio_opts & AUDIO_OPTS_LOCKED_FREEZE) ? TRUE : FALSE,
                         LIVES_BOX(hbox),
                         H_("Determines whether activating the freeze button affects audio "
                            "when the audio is locked "
                            "to a specific clip.\n\nNOTE: deactivating freeze will always "
                            "start the locked audio "
                            "regardless of this setting,\n"
                            "so for trick play you can lock audio to a video clip whilst it is frozen,\n"
                            "and then unfreeze both together (either by toggling the freeze button,\n"
                            "or by switching the video clip)"));

  show_warn_image(prefsw->afreeze_lock,
                  _("Only active when 'Audio follows video _rate/direction"));

  toggle_shows_warn_img(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow), prefsw->afreeze_lock, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_afollow), prefsw->afreeze_lock, FALSE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->auto_unlock = lives_standard_check_button_new(_("Unlock audio after playback ends"),
                        (prefs->audio_opts & AUDIO_OPTS_AUTO_UNLOCK) ? TRUE : FALSE,
                        LIVES_BOX(hbox),
                        H_("Setting this option will cause audio lock to always "
                           "disengage whenever playback finishes.\n"
                           "If unset then audio lock will remain active until "
                           "explicitly disabled."));

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->alock_reset = lives_standard_check_button_new(_("Reset playback rate for locked audio when fps is reset"),
                        (prefs->audio_opts & AUDIO_OPTS_LOCKED_RESET) ? TRUE : FALSE,
                        LIVES_BOX(hbox),
                        H_("If set, then resetting the playback fps for the current clip "
                           "will also reset the playback rate of clip-locked audio "
                           "even when locked to a different clip."));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->afreeze_ping = lives_standard_check_button_new(_("Ping pong mode affects locked audio"),
                         (prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG) ? TRUE : FALSE,
                         LIVES_BOX(hbox),
                         H_("Determines whether ping-pong mode affects audio "
                            "when the audio is locked to a specific clip.\n"
                            "If unset then audio direction will remain fixed when looping"));

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->afreeze_sync = lives_standard_check_button_new(_("Resync to current video position when unlocking audio"),
                         (prefs->audio_opts & AUDIO_OPTS_UNLOCK_RESYNC) ? TRUE : FALSE, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  SET_PREF_WIDGET(POGO_MODE,
                  lives_standard_check_button_new
                  (_("'POGO' mode audio locking"),
                   prefs->pogo_mode, LIVES_BOX(hbox),
                   H_("In 'POGO' mode, locked audio will be mixed with audio "
                      "from the current video clip, rather than completely overrding it")));

  add_hsep_to_box(LIVES_BOX(vbox));
  add_fill_to_box(LIVES_BOX(vbox));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
  label = lives_standard_label_new(_("Audio Source at start up (clip editor only):"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rintaudio = lives_standard_radio_button_new(_("_Internal"), &asrc_group, LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->rextaudio = lives_standard_radio_button_new(_("_External [monitor]"),
                      &asrc_group, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), future_prefs->audio_src == AUDIO_SRC_EXT);
  add_fill_to_box(LIVES_BOX(hbox));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->checkbutton_aclips, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->checkbutton_afollow, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->resync_fps, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->resync_vpos, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->resync_adir, TRUE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rextaudio), prefsw->resync_aclip, TRUE);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->rextaudio, FALSE);
  }

  add_fill_to_box(LIVES_BOX(vbox));

  pixbuf_playback = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_PLAYBACK, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_playback, _("Playback"), LIST_ENTRY_PLAYBACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_playback);

  lives_widget_hide(prefsw->jack_int_label);

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK) {
    lives_widget_set_no_show_all(prefsw->jack_int_label, FALSE);
    lives_widget_show_all(prefsw->jack_int_label);
  }
#endif

  // ---------------,
  // recording      |
  // ---------------'

  prefsw->vbox_right_recording = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_recording), widget_opts.border_width * 2);

  prefsw->scrollw_right_recording = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_recording);

  hbox = lives_hbox_new(FALSE, 0);
  prefsw->rdesk_audio = lives_standard_check_button_new(
                          _("Record audio when capturing an e_xternal window\n (requires jack or pulseaudio)"),
                          prefs->rec_desktop_audio, LIVES_BOX(hbox), NULL);

#ifndef RT_AUDIO
  lives_widget_set_sensitive(prefsw->rdesk_audio, FALSE);
  lives_widget_set_sensitive(GET_PREF_WIDGET(POGO_MODE), FALSE);
#endif

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("What to record when 'r' is pressed"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_recording));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rframes = lives_standard_check_button_new(_("_Frame changes"), (prefs->rec_opts & REC_FRAMES), LIVES_BOX(hbox), NULL);

  if (prefs->rec_opts & REC_FPS || prefs->rec_opts & REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rframes, FALSE); // we must record these if recording fps changes or clip switches
  }
  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rframes, FALSE);
  }

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rfps = lives_standard_check_button_new(_("F_PS changes"), (prefs->rec_opts & REC_FPS), LIVES_BOX(hbox), NULL);

  if (prefs->rec_opts & REC_CLIPS) {
    lives_widget_set_sensitive(prefsw->rfps, FALSE); // we must record these if recording clip switches
  }

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rfps, FALSE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->reffects = lives_standard_check_button_new(_("_Real time effects"), (prefs->rec_opts & REC_EFFECTS),
                     LIVES_BOX(hbox), NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->reffects, FALSE);
  }

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->rclips = lives_standard_check_button_new(_("_Clip switches"), (prefs->rec_opts & REC_CLIPS), LIVES_BOX(hbox), NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->rclips, FALSE);
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->raudio = lives_standard_check_button_new(_("_Audio (requires jack or pulseaudio player)"),
                   (prefs->rec_opts & REC_AUDIO), LIVES_BOX(hbox), NULL);

  if (mainw->playing_file > 0 && mainw->record) {
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
  }

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
  }

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->raudio_alock = lives_standard_check_button_new(_("AUTO _LOCK AUDIO ON RECORD START"),
                         (prefs->rec_opts & REC_AUDIO_AUTOLOCK), LIVES_BOX(hbox),
                         (tmp = H_("Setting this will cause audio to automatically "
                                   "become locked to the currently "
                                   "playing clip whenever recording "
                                   "begins.\nOnly functions when playing internal "
                                   "audio in Clip Edit mode.\n"
                                   "Pressing shift + A will unlock audio so that it may follow "
                                   "video track changes once more.\n"
                                   "Related: Playback / Audio follows video clip switches.")));
  lives_free(tmp);

  if (!is_realtime_aplayer(prefs->audio_player)) {
    lives_widget_set_sensitive(prefsw->raudio, FALSE);
  }

  lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_recording));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Pause recording if:"), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  // TODO - set to 0.
  lives_standard_check_button_new(_("Free disk space falls below"), TRUE, LIVES_BOX(hbox), NULL);

  widget_opts.swap_label = TRUE;

  // TRANSLATORS: gigabytes
  SET_PREF_WIDGET(REC_STOP_GB, lives_standard_spin_button_new(_("GB"), prefs->rec_stop_gb,
                  0., 1024., 1., 10., 0, LIVES_BOX(hbox), NULL));
  widget_opts.swap_label = FALSE;

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  // TODO - need implementing fully

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  // TODO - set to 0.
  cbut = lives_standard_check_button_new(_("More than"), TRUE, LIVES_BOX(hbox), NULL);

  widget_opts.swap_label = TRUE;

  // xgettext:no-c-format
  SET_PREF_WIDGET(REC_STOP_QUOTA, lives_standard_spin_button_new(_("% of quota is used"),
                  90., 0., 100., 1., 5., 0, LIVES_BOX(hbox), NULL));
  widget_opts.swap_label = FALSE;

  if (!prefs->disk_quota) {
    lives_widget_set_sensitive(cbut, FALSE);
    //lives_widget_set_sensitive(GET_PREF_WIDGET(REC_STOP_QUOTA), FALSE);
  }

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  SET_PREF_WIDGET(REC_STOP_DWARN, lives_standard_check_button_new(_("Disk space warning level is passed"),
                  TRUE, LIVES_BOX(hbox), NULL));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Recording is always paused if the disk space critical level is reached"),
                         FALSE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE,
                       widget_opts.packing_height);

  label = lives_standard_label_new(_("External Audio Source"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_recording), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->spinbutton_ext_aud_thresh = lives_standard_spin_button_new(
                                        _("Delay recording playback start until external audio volume reaches "),
                                        prefs->ahold_threshold * 100., 0., 100., 1., 10., 0,
                                        LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new("%");
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));

  // Rendering options

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_recording));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_recording));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Rendering Options (post Recording)"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->rr_crash = lives_standard_check_button_new(_("Enable Crash Recovery for recordings"), prefs->rr_crash,
                     LIVES_BOX(hbox),
                     (tmp = H_("If LiVES crashes or ctrl-c is pressed during "
                               "recording or when rendering a recording,"
                               "the recorded material will be retained and reloaded "
                               "the next time LiVES is restarted.\n\n")));
  ACTIVE(rr_crash, TOGGLED);

  if (!prefs->crash_recovery)
    show_warn_image(prefsw->rr_crash,
                    _("Crash recovery also needs to be enabled for this feature to function."));

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  widget_opts.use_markup = TRUE;
  prefsw->rr_super = lives_standard_check_button_new(_("Enable <b><u>SuperFluid</u></b> rendering:"),
                     prefs->rr_super, LIVES_BOX(hbox),
                     H_("Enables various pre-processing stages so that the resulting rendering\n"
                        "appears smoother and with fewer audio glitches"));
  widget_opts.use_markup = FALSE;

  ACTIVE(rr_super, TOGGLED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  rmodelist = lives_list_append(rmodelist, _("Prioritize Audio rate"));
  rmodelist = lives_list_append(rmodelist, _("Prioritize Frame rate"));
  prefsw->rr_combo = lives_standard_combo_new(_("Render mode:"), rmodelist, LIVES_BOX(hbox),
                     H_("In 'Prioritize Audio rate' mode, frame timings will be adjusted slightly "
                        "to maintain the correct audio rate\n"
                        "In 'Constant Frame Rate' mode, the audio rate will be adjusted slightly "
                        "so that frames are shown at the exact recorded time\n"));
  lives_list_free_all(&rmodelist);
  lives_combo_set_active_index(LIVES_COMBO(prefsw->rr_combo), prefs->rr_qmode);
  ACTIVE(rr_combo, CHANGED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->rr_pre_smooth = lives_standard_check_button_new(_("Enable pre-quantisation frame smoothing"), prefs->rr_pre_smooth,
                          LIVES_BOX(hbox), NULL);
  lives_widget_set_margin_left(widget_opts.last_container, widget_opts.packing_width * 4);
  ACTIVE(rr_pre_smooth, TOGGLED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->rr_qsmooth = lives_standard_check_button_new(_("Enable frame smoothing during quantisation"), prefs->rr_qsmooth,
                       LIVES_BOX(hbox), NULL);
  lives_widget_set_margin_left(widget_opts.last_container, widget_opts.packing_width * 4);
  ACTIVE(rr_qsmooth, TOGGLED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  radjlist = lives_list_append(radjlist, _("Apply effects state at original time"));
  radjlist = lives_list_append(radjlist, _("Apply effects state at adjusted time"));
  prefsw->rr_scombo = lives_standard_combo_new(_("When re-aligning frames"), radjlist, LIVES_BOX(hbox), NULL);
  lives_list_free_all(&radjlist);
  lives_combo_set_active_index(LIVES_COMBO(prefsw->rr_scombo), prefs->rr_fstate);
  ACTIVE(rr_scombo, CHANGED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->rr_amicro = lives_standard_check_button_new(_("Enable audio micro-adjustments during quantisation"), prefs->rr_amicro,
                      LIVES_BOX(hbox), NULL);
  lives_widget_set_margin_left(widget_opts.last_container, widget_opts.packing_width * 4);
  ACTIVE(rr_amicro, TOGGLED);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->rr_ramicro = lives_standard_check_button_new(_("Ignore tiny audio jumps during rendering"), prefs->rr_ramicro,
                       LIVES_BOX(hbox), NULL);
  lives_widget_set_margin_left(widget_opts.last_container, widget_opts.packing_width * 4);
  ACTIVE(rr_ramicro, TOGGLED);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_combo, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_scombo, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_combo, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_pre_smooth, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_qsmooth, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_amicro, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->rr_super), prefsw->rr_ramicro, FALSE);

  pixbuf_recording = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_RECORD, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_recording, _("Recording"), LIST_ENTRY_RECORDING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_recording);

  // ---------------,
  // encoding       |
  // ---------------'

  prefsw->vbox_right_encoding = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_encoding = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_encoding);

  widget_opts.packing_height <<= 2;
  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_encoding));

  lives_layout_add_label(LIVES_LAYOUT(layout),
                         _("You can also change these values when encoding a clip"), FALSE);

  if (ARE_PRESENT(encoder_plugins)) {
    // scan for encoder plugins
    encoders = capable->plugins_list[PLUGIN_TYPE_ENCODER];
  }

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->encoder_combo = lives_standard_combo_new(_("Encoder"), encoders,
                          LIVES_BOX(hbox), NULL);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  if (encoders) {
    lives_combo_set_active_string(LIVES_COMBO(prefsw->encoder_combo), prefs->encoder.name);
  }

  if (ARE_PRESENT(encoder_plugins)) {
    // request formats from the encoder plugin
    if ((ofmt_all = plugin_request_by_line(PLUGIN_ENCODERS, prefs->encoder.name, "get_formats"))) {
      for (i = 0; i < lives_list_length(ofmt_all); i++) {
        if (get_token_count((char *)lives_list_nth_data(ofmt_all, i), '|') > 2) {
          array = lives_strsplit((char *)lives_list_nth_data(ofmt_all, i), "|", -1);
          if (!strcmp(array[0], prefs->encoder.of_name)) {
            prefs->encoder.of_allowed_acodecs = lives_strtol(array[2]);
            lives_snprintf(prefs->encoder.of_restrict, 1024, "%s", array[3]);
          }
          ofmt = lives_list_append(ofmt, lives_strdup(array[1]));
          lives_strfreev(array);
        }
      }
      lives_memcpy(&future_prefs->encoder, &prefs->encoder, sizeof(_encoder));
    } else {
      do_plugin_encoder_error(prefs->encoder.name);
      future_prefs->encoder.of_allowed_acodecs = 0;
    }

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    prefsw->ofmt_combo = lives_standard_combo_new(_("Output format"), ofmt, LIVES_BOX(hbox), NULL);

    if (ofmt) {
      lives_combo_set_active_string(LIVES_COMBO(prefsw->ofmt_combo), prefs->encoder.of_desc);
      lives_list_free_all(&ofmt);
    }

    lives_list_free_all(&ofmt_all);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    prefsw->acodec_combo = lives_standard_combo_new(_("Audio codec"), NULL, LIVES_BOX(hbox), NULL);
    prefs->acodec_list = NULL;

    set_acodec_list_from_allowed(prefsw, rdet);

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    prefsw->checkbutton_lbenc = lives_standard_check_button_new(_("Apply letterboxing when encoding or transcoding"),
                                prefs->enc_letterbox, LIVES_BOX(hbox), NULL);
  } else prefsw->acodec_combo = NULL;
  widget_opts.packing_height >>= 2;

  pixbuf_encoding = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_ENCODING, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_encoding, _("Encoding"), LIST_ENTRY_ENCODING);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_encoding);

  // ---------------,
  // effects        |
  // ---------------'

  prefsw->vbox_right_effects = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_effects = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_effects);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_effects));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_load_rfx = lives_standard_check_button_new(_("Load rendered effects on startup"), prefs->load_rfx_builtin,
                                 LIVES_BOX(hbox), NULL);

  if (prefs->vj_mode)
    show_warn_image(prefsw->checkbutton_load_rfx, _("Disabled in VJ mode"));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_antialias = lives_standard_check_button_new(_("Use _antialiasing when resizing"), prefs->antialias,
                                  LIVES_BOX(hbox), H_("This setting applies to images imported into or\n"
                                      "exported from LiVES, and for certain rendered effects\n"
                                      "and Tools, such as Trim Frames"));

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_effects));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_effects));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_rte_keys = lives_standard_spin_button_new
                                ((tmp = (_("Number of _real time effect keys"))), prefs->rte_keys_virtual, FX_KEYS_PHYSICAL,
                                 FX_KEYS_MAX_VIRTUAL, 1., 1., 0, LIVES_BOX(hbox),
                                 (tmp2 = H_("The number of \"virtual\" real time effect keys.\n"
                                         "They can be controlled through the real time effects window,\n"
                                         "or via network (OSC), or other means.")));
  lives_free(tmp); lives_free(tmp2);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_rte_modes = lives_standard_spin_button_new
                                 ((tmp = (_("Modes per key"))), prefs->rte_modes_per_key, 1.,
                                  FX_MODES_MAX, 1., 1., 0, LIVES_BOX(hbox),
                                  (tmp2 = H_("Each effect key can have multiple modes which can each be assigned to a\n"
                                          "realtime effect, for example, by means of the effect mapping window\n"
                                          "These modes can then be cycled through (see VJ / Show VJ keys for an example).")));
  lives_free(tmp); lives_free(tmp2);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_threads = lives_standard_check_button_new(_("Use _threads where possible when applying effects"),
                                future_prefs->nfx_threads > 1, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_nfx_threads = lives_standard_spin_button_new(_("Number of _threads"), future_prefs->nfx_threads, 2., 65536.,
                                   1., 1., 0, LIVES_BOX(hbox), NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_threads), prefsw->spinbutton_nfx_threads, FALSE);

  add_fill_to_box(LIVES_BOX(hbox));
  add_fill_to_box(LIVES_BOX(hbox));

  if (future_prefs->nfx_threads == 1) lives_widget_set_sensitive(prefsw->spinbutton_nfx_threads, FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->pa_gens = lives_standard_check_button_new(_("Push audio to video generators that support it"),
                    prefs->push_audio_to_gens, LIVES_BOX(hbox), NULL);


  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  SET_PREF_WIDGET(SELF_TRANS,
                  lives_standard_check_button_new(_("Allow clips to transition with themselves"),
                      prefs->tr_self, LIVES_BOX(hbox),
                      (tmp = H_("Enabling this allows clips to blend with themselves "
                                "during realtime playback.\nSeparate effects can then be applied "
                                "to each copy (background and foreground),\nand the result will be "
                                "created by combining these separate copies.\n"
                                "If unset, enabling a "
                                "transition filter will do nothing until a "
                                "new background clip is selected."))));
  lives_free(tmp);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_apply_gamma =
    lives_standard_check_button_new(_("Automatic gamma correction (requires restart)"),
                                    prefs->apply_gamma, LIVES_BOX(hbox),
                                    (tmp = (_("Also affects the monitor gamma !! (for now...)"))));
  lives_free(tmp);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_effects));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_effects), hbox, FALSE, FALSE,
                       widget_opts.packing_height);

  label = lives_standard_label_new(_("Restart is required if any of the following paths are changed:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, TRUE, TRUE, 0);

  add_fill_to_box(LIVES_BOX(hbox));

  widget_opts.packing_height *= 2;

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_effects));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Changing these values will only take effect after a restart of LiVES:"), FALSE);

  widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->wpp_entry = lives_standard_direntry_new(_("Weed plugin path"), prefs->weed_plugin_path,
                      MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                      (tmp = H_("Weed directories should be separated by ':',\n"
                                "ordered from lowest to highest priority")));
  lives_free(tmp);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->frei0r_entry = lives_standard_direntry_new(_("Frei0r plugin path"), prefs->frei0r_path,
                         MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                         (tmp = H_("Frei0r directories should be separated by ':',\n"
                                   "ordered from lowest to highest priority")));
  lives_free(tmp);

#ifndef HAVE_FREI0R
  lives_widget_set_sensitive(prefsw->frei0r_entry, FALSE);
  show_warn_image(prefsw->frei0r_entry, _("LiVES was compiled without frei0r support"));
#endif

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->ladspa_entry = lives_standard_direntry_new(_("LADSPA plugin path"), prefs->ladspa_path, MEDIUM_ENTRY_WIDTH, PATH_MAX,
                         LIVES_BOX(hbox), NULL);
#ifndef HAVE_LADSPA
  lives_widget_set_sensitive(prefsw->ladspa_entry, FALSE);
  show_warn_image(prefsw->ladspa_entry, _("LiVES was compiled without LADSPA support"));
#endif

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->libvis_entry = lives_standard_direntry_new(_("libvisual plugin path"), prefs->libvis_path, MEDIUM_ENTRY_WIDTH, PATH_MAX,
                         LIVES_BOX(hbox), NULL);

  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.packing_height = woph;

#ifndef HAVE_LIBVISUAL
  lives_widget_set_sensitive(prefsw->libvis_entry, FALSE);
  show_warn_image(prefsw->libvis_entry, _("LiVES was compiled without libvisual support"));
#endif

  pixbuf_effects = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_EFFECTS, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_effects, _("Effects"), LIST_ENTRY_EFFECTS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_effects);

  // -------------------,
  // Directories        |
  // -------------------'

  prefsw->table_right_directories = lives_table_new(10, 3, FALSE);

  lives_container_set_border_width(LIVES_CONTAINER(prefsw->table_right_directories), widget_opts.border_width * 2);
  lives_table_set_col_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_width * 2);
  lives_table_set_row_spacings(LIVES_TABLE(prefsw->table_right_directories), widget_opts.packing_height * 4);

  prefsw->scrollw_right_directories = lives_standard_scrolled_window_new(0, 0, prefsw->table_right_directories);

  widget_opts.justify = LIVES_JUSTIFY_END;
  label = lives_standard_label_new(_("Video load directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 4, 5,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Video save directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 5, 6,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Audio load directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 6, 7,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Image directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 7, 8,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  label = lives_standard_label_new(_("Backup/Restore directory (default)"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 8, 9,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(_("<b>Work directory</b>"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), label, 0, 1, 3, 4,
                     (LiVESAttachOptions)(LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);
  widget_opts.use_markup = FALSE;
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  // workdir warning label
  layout = lives_layout_new(NULL);

  prefsw->workdir_label = lives_layout_add_label(LIVES_LAYOUT(layout), NULL, FALSE);
  set_workdir_label_text(LIVES_LABEL(prefsw->workdir_label), prefs->workdir);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), layout, 0, 3, 0, 2,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  prefsw->workdir_entry =
    lives_standard_entry_new(NULL, *future_prefs->workdir
                             ? future_prefs->workdir : prefs->workdir,
                             -1, PATH_MAX, NULL,
                             (tmp2 = _("The default directory for saving encoded clips to")));
  lives_free(tmp2);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->workdir_entry, 1, 2, 3, 4,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->workdir_entry), FALSE);
  ACTIVE(workdir_entry, CHANGED);

  prefsw->vid_load_dir_entry = lives_standard_entry_new(NULL, prefs->def_vid_load_dir, -1, PATH_MAX,
                               NULL,  _("The default directory for loading video clips from"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_load_dir_entry, 1, 2, 4, 5,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->vid_load_dir_entry), FALSE);
  ACTIVE(vid_load_dir_entry, CHANGED);

  prefsw->vid_save_dir_entry = lives_standard_entry_new(NULL, prefs->def_vid_save_dir, -1, PATH_MAX,
                               NULL, _("The default directory for saving encoded clips to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->vid_save_dir_entry, 1, 2, 5, 6,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->vid_save_dir_entry), FALSE);
  ACTIVE(vid_save_dir_entry, CHANGED);

  prefsw->audio_dir_entry = lives_standard_entry_new(NULL, prefs->def_audio_dir, -1, PATH_MAX,
                            NULL, _("The default directory for loading and saving audio"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->audio_dir_entry, 1, 2, 6, 7,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->audio_dir_entry), FALSE);
  ACTIVE(audio_dir_entry, CHANGED);

  prefsw->image_dir_entry = lives_standard_entry_new(NULL, prefs->def_image_dir, -1, PATH_MAX,
                            NULL, _("The default directory for saving frameshots to"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->image_dir_entry, 1, 2, 7, 8,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->image_dir_entry), FALSE);
  ACTIVE(image_dir_entry, CHANGED);

  prefsw->proj_dir_entry = lives_standard_entry_new(NULL, prefs->def_proj_dir, -1, PATH_MAX,
                           NULL, _("The default directory for backing up/restoring single clips"));
  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), prefsw->proj_dir_entry, 1, 2, 8, 9,
                     (LiVESAttachOptions)(LIVES_EXPAND | LIVES_FILL),
                     (LiVESAttachOptions)(0), 0, 0);

  lives_entry_set_editable(LIVES_ENTRY(prefsw->proj_dir_entry), FALSE);
  ACTIVE(proj_dir_entry, CHANGED);

  /// dirbuttons

  dirbutton = lives_standard_file_button_new(TRUE, *future_prefs->workdir
              ? future_prefs->workdir : prefs->workdir);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 3, 4,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(dirbutton), FILESEL_TYPE_KEY,
                               (livespointer)LIVES_DIR_SELECTION_WORKDIR);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_filesel_button_clicked), prefsw->workdir_entry);

  if (mainw->has_session_workdir) {
    show_warn_image(prefsw->workdir_entry, _("Value was set via commandline option"));
    prefsw->checkbutton_perm_workdir = lives_standard_check_button_new
                                       (_("Make ths value permanent"), !(prefs->warning_mask & WARN_MASK_FSIZE), LIVES_BOX(hbox),
                                        H_("Check this to make the -workdir value supplied on the commandline become the permanent value"));
    ACTIVE(checkbutton_perm_workdir, TOGGLED);
  } else if (prefs->vj_mode) {
    show_warn_image(prefsw->workdir_entry, _("Changes disabled in VJ mode"));
    lives_widget_set_sensitive(dirbutton, FALSE);
  }


  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 4, 5,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->vid_load_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 5, 6,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->vid_save_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 6, 7,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->audio_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 7, 8,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->image_dir_entry);

  dirbutton = lives_standard_file_button_new(TRUE, NULL);

  lives_table_attach(LIVES_TABLE(prefsw->table_right_directories), dirbutton, 2, 3, 8, 9,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_signal_sync_connect(dirbutton, LIVES_WIDGET_CLICKED_SIGNAL, LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->proj_dir_entry);

  pixbuf_directories = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_DIRECTORY, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_directories, _("Directories"), LIST_ENTRY_DIRECTORIES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_directories);

  // ---------------,
  // Warnings       |
  // ---------------'

  prefsw->vbox_right_warnings = lives_vbox_new(FALSE, widget_opts.packing_height >> 2);
  prefsw->scrollw_right_warnings =
    lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_warnings);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_warnings));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Low Disk Space Warnings (set to zero to disable)"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_warn_ds =
    lives_standard_spin_button_new((tmp = (_("Disk space warning level"))),
                                   prefs->ds_warn_level / MILLIONS(1), prefs->ds_crit_level / MILLIONS(1), DS_WARN_CRIT_MAX,
                                   1., 50., 1,
                                   LIVES_BOX(hbox), (tmp2 = (H_("LiVES will start showing warnings if usable disk space\n"
                                       "falls below this level"))));
  lives_free(tmp); lives_free(tmp2);

  if (prefs->vj_mode)
    show_warn_image(prefsw->spinbutton_warn_ds, _("Reduced checking in VJ mode"));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("MB"), TRUE);

  tmp = lives_format_storage_space_string(prefs->ds_warn_level);
  tmp2 = lives_strdup_printf("(%s)", tmp);
  prefsw->dsl_label = lives_layout_add_label(LIVES_LAYOUT(layout), tmp2, TRUE);
  lives_free(tmp); lives_free(tmp2);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->spinbutton_crit_ds =
    lives_standard_spin_button_new((tmp = (_("Disk space critical level"))),
                                   prefs->ds_crit_level / MILLIONS(1), 0, MILLIONS(1), 1., 10., 1,
                                   LIVES_BOX(hbox), (tmp2 = (H_("LiVES will abort if usable disk space\n"
                                       "falls below this level"))));
  lives_free(tmp); lives_free(tmp2);

  if (prefs->vj_mode)
    show_warn_image(prefsw->spinbutton_crit_ds, _("Reduced checking in VJ mode"));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("MB"), TRUE);

  tmp = lives_format_storage_space_string(prefs->ds_crit_level);
  tmp2 = lives_strdup_printf("(%s)", tmp);
  prefsw->dsc_label = lives_layout_add_label(LIVES_LAYOUT(layout), tmp2, TRUE);
  lives_free(tmp); lives_free(tmp2);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_warnings));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_warnings));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  //hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_fps = lives_standard_check_button_new
                                 (_("Warn on Insert / Merge if _frame rate of clipboard does "
                                    "not match frame rate of selection"),
                                  !(prefs->warning_mask & WARN_MASK_FPS), LIVES_BOX(hbox), NULL);


  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_fsize = lives_standard_check_button_new
                                   (_("Warn on Open if Instant Opening is not available, and the file _size exceeds "),
                                    !(prefs->warning_mask & WARN_MASK_FSIZE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->spinbutton_warn_fsize = lives_standard_spin_button_new
                                  (NULL, prefs->warn_file_size, 1., 2048., 1., 10., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  label = lives_standard_label_new(_(" MB"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width >> 1);

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);
  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_save_set =
    lives_standard_check_button_new
    (_("Show a warning before saving a se_t"),
     !(prefs->warning_mask & WARN_MASK_SAVE_SET), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mplayer = lives_standard_check_button_new
                                     (_("Show a warning if _mplayer/mplayer2, sox, composite or convert is not "
                                        "found when LiVES is started."),
                                      !(prefs->warning_mask & WARN_MASK_NO_MPLAYER), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_prefix = lives_standard_check_button_new
                                    (_("Show a warning if prefix__dir setting is wrong at startup"),
                                     !(prefs->warning_mask & WARN_MASK_CHECK_PREFIX),
                                     LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_missplugs = lives_standard_check_button_new
                                       (_("Show a warning if some _plugin types are not found at startup."),
                                        !(prefs->warning_mask & WARN_MASK_CHECK_PLUGINS),
                                        LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_dup_set = lives_standard_check_button_new
                                     (_("Show a warning if a _duplicate set name is entered."),
                                      !(prefs->warning_mask & WARN_MASK_DUPLICATE_SET), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_clips = lives_standard_check_button_new
                                          (_("When a set is loaded, warn if clips are missing from _layouts."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_MISSING_CLIPS), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_close = lives_standard_check_button_new
                                          (_("Warn if a clip used in a layout is about to be closed."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_CLOSE_FILE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_delete = lives_standard_check_button_new
      (_("Warn if frames used in a layout are about to be deleted."),
       !(prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_shift = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be shifted."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_alter = lives_standard_check_button_new
                                          (_("Warn if frames used in a layout are about to be altered."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_FRAMES), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_adel = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be deleted."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_DELETE_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_ashift = lives_standard_check_button_new
      (_("Warn if audio used in a layout is about to be shifted."),
       !(prefs->warning_mask & WARN_MASK_LAYOUT_SHIFT_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_aalt = lives_standard_check_button_new
                                         (_("Warn if audio used in a layout is about to be altered."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_ALTER_AUDIO), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_popup = lives_standard_check_button_new
                                          (_("Popup layout errors after clip changes."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_POPUP), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_reload = lives_standard_check_button_new
      (_("Popup layout errors after reloading a set."), !(prefs->warning_mask & WARN_MASK_LAYOUT_RELOAD), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_discard_layout = lives_standard_check_button_new
      (_("Warn if the layout has not been saved when leaving multitrack mode."),
       !(prefs->warning_mask & WARN_MASK_EXIT_MT), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_achans = lives_standard_check_button_new
                                       (_("Warn if multitrack has no audio channels, and a layout with audio is loaded."),
                                        !(prefs->warning_mask & WARN_MASK_MT_ACHANS), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_no_jack = lives_standard_check_button_new
                                        (_("Warn if multitrack has audio channels, and your audio player is not \"jack\" or \"pulseaudio\"."),
                                         !(prefs->warning_mask & WARN_MASK_MT_NO_JACK), LIVES_BOX(hbox), NULL);

#ifdef HAVE_LDVGRAB
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_after_dvgrab = lives_standard_check_button_new
                                          (_("Show info message after importing from firewire device."),
                                              !(prefs->warning_mask & WARN_MASK_AFTER_DVGRAB), LIVES_BOX(hbox), NULL);

#else
  prefsw->checkbutton_warn_after_dvgrab = NULL;
#endif

#ifdef HAVE_YUV4MPEG
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_yuv4m_open = lives_standard_check_button_new
                                        (_("Show a warning before opening a yuv4mpeg stream (advanced)."),
                                         !(prefs->warning_mask & WARN_MASK_OPEN_YUV4M), LIVES_BOX(hbox), NULL);
#else
  prefsw->checkbutton_warn_yuv4m_open = NULL;
#endif

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_mt_backup_space = lives_standard_check_button_new
      (_("Show a warning when multitrack is low on backup space."),
       !(prefs->warning_mask & WARN_MASK_MT_BACKUP_SPACE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_after_crash = lives_standard_check_button_new
                                         (_("Show a warning advising cleaning of disk space after a crash."),
                                          !(prefs->warning_mask & WARN_MASK_CLEAN_AFTER_CRASH), LIVES_BOX(hbox), NULL);
  if (prefs->vj_mode)
    show_warn_image(prefsw->checkbutton_warn_after_crash,
                    _("Warning is never shown in VJ startup mode."));
  else if (prefs->show_dev_opts)
    show_warn_image(prefsw->checkbutton_warn_after_crash,
                    _("Warning is never shown in developer mode."));

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_no_pulse = lives_standard_check_button_new
                                      (_("Show a warning if unable to connect to pulseaudio player."),
                                       !(prefs->warning_mask & WARN_MASK_NO_PULSE_CONNECT), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_wipe = lives_standard_check_button_new
                                         (_("Show a warning before wiping a layout which has unsaved changes."),
                                          !(prefs->warning_mask & WARN_MASK_LAYOUT_WIPE), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_gamma = lives_standard_check_button_new
                                          (_("Show a warning if a loaded layout has incompatible gamma settings."),
                                              !(prefs->warning_mask & WARN_MASK_LAYOUT_GAMMA), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_layout_lb = lives_standard_check_button_new
                                       (_("Show a warning if a loaded layout has incompatible letterbox settings."),
                                        !(prefs->warning_mask & WARN_MASK_LAYOUT_LB), LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_vjmode_enter = lives_standard_check_button_new
                                          (_("Show a warning when the menu option Restart in VJ Mode becomes activated."),
                                              !(prefs->warning_mask & WARN_MASK_VJMODE_ENTER), LIVES_BOX(hbox), NULL);

#ifdef ENABLE_JACK
  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_jack_scrpts = lives_standard_check_button_new
                                         (_("Show if multiple jack clients are set to "
                                             "start a server using the same script file."),
                                          !(prefs->warning_mask & WARN_MASK_JACK_SCRPT),
                                          LIVES_BOX(hbox), NULL);
#endif

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_dmgd_audio = lives_standard_check_button_new
                                        (_("Show a warning after opening a clip with damaged audio "
                                            "and allow attempted recovery."),
                                         !(prefs->warning_mask & WARN_MASK_DMGD_AUDIO),
                                         LIVES_BOX(hbox), NULL);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_warn_dis_dec = lives_standard_check_button_new
                                     (_("Show a warning if a clip to be reloaded was using a disabled decoder plugin."),
                                      !(prefs->warning_mask & WARN_MASK_DISABLED_DECODER), LIVES_BOX(hbox), NULL);

  pixbuf_warnings = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_WARNING,
                    LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_warnings, _("Warnings"), LIST_ENTRY_WARNINGS);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_warnings);

  // -----------,
  // Misc       |
  // -----------'

  prefsw->vbox_right_misc = lives_vbox_new(FALSE, widget_opts.packing_height * 4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_misc), widget_opts.border_width * 2);

  prefsw->scrollw_right_misc = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_misc);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  label = lives_standard_label_new(_("When inserting/merging frames:"));

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);
  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->ins_speed = lives_standard_radio_button_new(_("_Speed Up/Slow Down Insertion"), &rb_group2, LIVES_BOX(hbox), NULL);

  ins_resample = lives_standard_radio_button_new(_("_Resample Insertion"), &rb_group2, LIVES_BOX(hbox), NULL);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(ins_resample), prefs->ins_resample);

  prefsw->cdda_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), prefsw->cdda_hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->cdplay_entry = lives_standard_fileentry_new((tmp = (_("CD device"))),
                         (tmp2 = lives_filename_to_utf8(prefs->cdplay_device, -1, NULL, NULL, NULL)),
                         LIVES_DEVICE_DIR, MEDIUM_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(prefsw->cdda_hbox),
                         (tmp3 = (_("LiVES can load audio tracks from this CD"))));
  lives_free(tmp); lives_free(tmp2); lives_free(tmp3);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_def_fps = lives_standard_spin_button_new((tmp = (_("Default FPS"))),
                               prefs->default_fps, 1., FPS_MAX, 1., 1., 3,
                               LIVES_BOX(hbox), (tmp2 = (_("Frames per second to use when none is specified"))));
  lives_free(tmp); lives_free(tmp2);

  pixbuf_misc = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MISC, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_misc, _("Misc"), LIST_ENTRY_MISC);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_misc);

  if (!check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)
      && !check_for_executable(&capable->has_icedax, EXEC_ICEDAX)) {
    lives_widget_hide(prefsw->cdda_hbox);
  }

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_misc));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE,
                       widget_opts.packing_height);

  prefsw->cb_show_quota = lives_standard_check_button_new
                          (_("Pop up disk quota settings window on startup"), prefs->show_disk_quota, LIVES_BOX(hbox), NULL);
  ACTIVE(cb_show_quota, TOGGLED);
  if (mainw->has_session_workdir)
    show_warn_image(prefsw->cb_show_quota, _("Quota checking is disabled when workdir is set\n"
                    "via commandline option"));
  else if (prefs->vj_mode)
    show_warn_image(prefsw->cb_show_quota, _("Disabled in VJ mode"));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE,
                       widget_opts.packing_height);

  prefsw->cb_show_msgstart = lives_standard_check_button_new
                             (_("Pop up messages window on startup"), prefs->show_msgs_on_startup, LIVES_BOX(hbox), NULL);
  ACTIVE(cb_show_msgstart, TOGGLED);
  if (prefs->vj_mode)
    show_warn_image(prefsw->cb_show_msgstart, _("Disabled in VJ mode"));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_misc), hbox, FALSE, FALSE,
                       widget_opts.packing_height);

  prefsw->cb_autoclean = lives_standard_check_button_new
                         (_("_Remove temporary backup files on startup and shutdown"),
                          prefs->autoclean, LIVES_BOX(hbox), H_("Save disk space by "
                              "allowing LiVES to remove\ntemporary preview and backup files"));
  ACTIVE(cb_autoclean, TOGGLED);

  if (prefs->vj_mode)
    show_warn_image(prefsw->rb_startup_mt, _("Disabled in VJ mode"));

  // -----------,
  // Themes     |
  // -----------'

  prefsw->vbox_right_themes = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_themes), widget_opts.border_width * 2);

  prefsw->scrollw_right_themes = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_themes);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), hbox, TRUE, FALSE, widget_opts.packing_height);

  // scan for themes
  themes = get_plugin_list(PLUGIN_THEMES_CUSTOM, TRUE, NULL, NULL);
  themes = lives_list_concat(themes, get_plugin_list(PLUGIN_THEMES, TRUE, NULL, NULL));
  themes = lives_list_prepend(themes, lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]));

  prefsw->theme_combo = lives_standard_combo_new(_("New theme:           "), themes, LIVES_BOX(hbox), NULL);

  if (strcasecmp(future_prefs->theme, LIVES_THEME_NONE)) {
    theme = lives_strdup(future_prefs->theme);
  } else theme = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_NONE]);

  lives_list_free_all(&themes);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_themes));

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_themes));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Eye Candy"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_button_icons =
    lives_standard_check_button_new(_("Show icons in buttons"), widget_opts.show_button_images,
                                    LIVES_BOX(hbox), NULL);

#if LIVES_HAS_IMAGE_MENU_ITEM
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_menu_icons =
    lives_standard_check_button_new(_("Show icons in menus"), prefs->show_menu_images,
                                    LIVES_BOX(hbox), NULL);
#else
  prefsw->checkbutton_menu_icons = NULL;
#endif

  lives_layout_add_fill(LIVES_LAYOUT(layout), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_extra_colours =
    lives_standard_check_button_new(_("Supplement interface colours"),
                                    prefs->extra_colours, LIVES_BOX(hbox),
                                    (tmp = H_("Make the interface more interesting "
                                        "by adding extra harmonious colours")));
  lives_free(tmp);

  frame = lives_standard_frame_new(_("Main Theme Details"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget_color_to_lives_rgba(&rgba, &palette->normal_fore);
  prefsw->cbutton_fore = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Foreground Color"), FALSE, &rgba, &sp_red,
                         &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_fore, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->normal_back);
  prefsw->cbutton_back = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Background Color"), FALSE, &rgba, &sp_red,
                         &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_back, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->menu_and_bars_fore);
  prefsw->cbutton_mabf = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Alt Foreground Color"), FALSE, &rgba, &sp_red,
                         &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_mabf, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->menu_and_bars);
  prefsw->cbutton_mab = lives_standard_color_button_new(LIVES_BOX(hbox), _("_Alt Background Color"), FALSE, &rgba, &sp_red,
                        &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_mab, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->info_text);
  prefsw->cbutton_infot = lives_standard_color_button_new(LIVES_BOX(hbox), _("Info _Text Color"), FALSE, &rgba, &sp_red,
                          &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_infot, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->info_base);
  prefsw->cbutton_infob = lives_standard_color_button_new(LIVES_BOX(hbox), _("Info _Base Color"), FALSE, &rgba, &sp_red,
                          &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_infob, COLOR_SET);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
  prefsw->theme_style3 = lives_standard_check_button_new((tmp = (_("Theme is _light"))), (palette->style & STYLE_3),
                         LIVES_BOX(hbox),
                         (tmp2 = (_("Affects some contrast details of the timeline"))));
  lives_free(tmp); lives_free(tmp2);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  prefsw->theme_style2 = NULL;
#if GTK_CHECK_VERSION(3, 0, 0)
  prefsw->theme_style2 = lives_standard_check_button_new(_("Color the start/end frame spinbuttons (requires restart)"),
                         (palette->style & STYLE_2), LIVES_BOX(hbox), NULL);
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);

  prefsw->theme_style4 = lives_standard_check_button_new(_("Highlight horizontal separators in multitrack"),
                         (palette->style & STYLE_4), LIVES_BOX(hbox), NULL);
  layout = lives_layout_new(LIVES_BOX(vbox));

  filt = (char **)lives_malloc(3 * sizeof(char *));
  filt[0] = LIVES_FILE_EXT_JPG;
  filt[1] = LIVES_FILE_EXT_PNG;
  filt[2] = NULL;

  lives_layout_add_label(LIVES_LAYOUT(layout), _("Frame blank image"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->frameblank_entry = lives_standard_fileentry_new(" ", mainw->frameblank_path,
                             prefs->def_image_dir, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                             (tmp2 = (_("The frame image which is shown when there is no clip loaded."))));
  lives_free(tmp2);

  prefsw->fb_filebutton =
    (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(prefsw->frameblank_entry), BUTTON_KEY);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->fb_filebutton), FILTER_KEY, filt);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->fb_filebutton), FILESEL_TYPE_KEY,
                               LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Separator image"), TRUE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->sepimg_entry = lives_standard_fileentry_new(" ", mainw->sepimg_path,
                         prefs->def_image_dir, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                         (tmp2 = (_("The image shown in the center of the interface."))));
  lives_free(tmp2);

  prefsw->se_filebutton =
    (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(prefsw->sepimg_entry), BUTTON_KEY);
  lives_signal_sync_connect(prefsw->se_filebutton, LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_filesel_button_clicked),
                            prefsw->sepimg_entry);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->se_filebutton), FILTER_KEY, filt);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(prefsw->se_filebutton), FILESEL_TYPE_KEY,
                               LIVES_INT_TO_POINTER(LIVES_FILE_SELECTION_IMAGE_ONLY));

  lives_free(filt);

  frame = lives_standard_frame_new(_("Extended Theme Details"), 0., FALSE);

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_themes), frame, TRUE, TRUE, widget_opts.packing_height);

  vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(frame), vbox);
  lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width);

  layout = lives_layout_new(LIVES_BOX(vbox));
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_cesel = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Selected frames/audio (clip editor)"))),
                          FALSE, &palette->ce_sel, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_cesel, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_ceunsel = lives_standard_color_button_new(LIVES_BOX(hbox),
                            (tmp = (_("Unselected frames/audio (clip editor)"))),
                            FALSE, &palette->ce_unsel, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_ceunsel, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_evbox = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Track background (multitrack)"))),
                          FALSE, &palette->mt_evbox, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_evbox, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_vidcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Video block (multitrack)"))),
                           FALSE, &palette->vidcol, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_vidcol, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_audcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Audio block (multitrack)"))),
                           FALSE, &palette->audcol, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_audcol, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_fxcol = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Effects block (multitrack)"))),
                          FALSE, &palette->fxcol, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_fxcol, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_mtmark = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Timeline mark (multitrack)"))),
                           FALSE, &palette->mt_mark, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_mtmark, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_tlreg = lives_standard_color_button_new(LIVES_BOX(hbox),
                          (tmp = (_("Timeline selection (multitrack)"))),
                          FALSE, &palette->mt_timeline_reg, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_tlreg, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->mt_timecode_bg);
  prefsw->cbutton_tcbg = lives_standard_color_button_new(LIVES_BOX(hbox),
                         (tmp = (_("Timecode background (multitrack)"))),
                         FALSE, &rgba, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_tcbg, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  widget_color_to_lives_rgba(&rgba, &palette->mt_timecode_fg);
  prefsw->cbutton_tcfg = lives_standard_color_button_new(LIVES_BOX(hbox),
                         (tmp = (_("Timecode foreground (multitrack)"))),
                         FALSE, &rgba, &sp_red, &sp_green, &sp_blue, NULL);
  ACTIVE(cbutton_tcfg, COLOR_SET);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->cbutton_fsur = lives_standard_color_button_new(LIVES_BOX(hbox), (tmp = (_("Frame surround"))),
                         FALSE, &palette->frame_surround, &sp_red, &sp_green, &sp_blue, NULL);
  lives_free(tmp);
  ACTIVE(cbutton_fsur, COLOR_SET);

  // change in value of theme combo should set other widgets sensitive / insensitive
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->theme_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(theme_widgets_set_sensitive), (livespointer)prefsw);
  lives_combo_set_active_string(LIVES_COMBO(prefsw->theme_combo), theme);
  lives_free(theme);

  pixbuf_themes = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_THEMES, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_themes, _("Themes/Colors"), LIST_ENTRY_THEMES);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_themes);

  // --------------------------,
  // streaming/networking      |
  // --------------------------'

  prefsw->vbox_right_net = lives_vbox_new(FALSE, widget_opts.packing_height * 4);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_net), widget_opts.border_width * 2);

  prefsw->scrollw_right_net = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_net);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_bwidth = lives_standard_spin_button_new(_("Download bandwidth (Kb/s)"),
                              prefs->dl_bandwidth, 0, 100000000., 1, 10, 0, LIVES_BOX(hbox), NULL);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_net));

#ifndef ENABLE_OSC
  label = lives_standard_label_new(_("LiVES must be compiled without \"configure --disable-OSC\" to use OMC"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), label, FALSE, FALSE, widget_opts.packing_height);
#endif

  hbox1 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox1, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox1), hbox, FALSE, FALSE, 0);

  prefsw->enable_OSC = lives_standard_check_button_new(_("OMC remote control enabled"), FALSE, LIVES_BOX(hbox), NULL);

#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->enable_OSC, FALSE);
#endif

  prefsw->spinbutton_osc_udp = lives_standard_spin_button_new(_("UDP port"),
                               prefs->osc_udp_port, 1., 65535., 1., 10., 0, LIVES_BOX(hbox), NULL);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_net), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->enable_OSC_start = lives_standard_check_button_new(_("Start OMC on startup"), future_prefs->osc_start,
                             LIVES_BOX(hbox), NULL);

#ifndef ENABLE_OSC
  lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, FALSE);
  lives_widget_set_sensitive(prefsw->enable_OSC_start, FALSE);
  lives_widget_set_sensitive(label, FALSE);
#else
  if (prefs->osc_udp_started) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->enable_OSC), TRUE);
    lives_widget_set_sensitive(prefsw->spinbutton_osc_udp, FALSE);
    lives_widget_set_sensitive(prefsw->enable_OSC, FALSE);
  }
#endif

  pixbuf_net = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_NET, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_net, _("Streaming/Networking"), LIST_ENTRY_NET);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_net);

  // ----------,
  // jack      |
  // ----------'

  prefsw->vbox_right_jack = lives_vbox_new(FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_jack),
                                   widget_opts.border_width * 2);

  prefsw->scrollw_right_jack = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_jack);

  layout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_jack));
  tmp = lives_big_and_bold("%s", _("Jack Audio"));
  widget_opts.use_markup = TRUE;
  prefsw->jack_aplabel = lives_layout_add_label(LIVES_LAYOUT(layout), tmp, TRUE);
  widget_opts.use_markup = FALSE;
  lives_free(tmp);

#ifndef ENABLE_JACK
  show_warn_image(prefsw->jack_aplabel,
                  _("LiVES must be compiled with libjack support to use jack audio"));
#else
  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  prefsw->jack_apstats = lives_standard_button_new_with_label(_("Show Client Status"), -1, -1);
  lives_box_pack_start(LIVES_BOX(hbox), prefsw->jack_apstats, FALSE, TRUE, widget_opts.packing_width);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jack_apstats), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(show_jack_status), LIVES_INT_TO_POINTER(0));

  ajack_cfg_exists = jack_get_cfg_file(FALSE, NULL);

  if (future_prefs->jack_opts & (JACK_INFO_TEMP_OPTS | JACK_INFO_TEMP_NAMES)) {
    hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
    add_fill_to_box(LIVES_BOX(hbox));
    widget_opts.use_markup = TRUE;
    prefsw->jack_apperm =
      lives_standard_button_new_from_stock_full(LIVES_STOCK_APPLY,
          _("Some values were set via commandline - <b>Click Here</b> to make them permanent"),
          -1, -1, LIVES_BOX(hbox), TRUE, NULL);
    widget_opts.use_markup = FALSE;
    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jack_apperm), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(jack_make_perm), NULL);
  }

  layout = prefsw->jack_aplayout = lives_layout_new(LIVES_BOX(prefsw->vbox_right_jack));
  show_warn_image(prefsw->jack_aplabel, _("You MUST set the audio player to 'jack'"
                                          "in the Playback tab to use jack audio"));

  if (prefs->audio_player == AUD_PLAYER_JACK) {
    hide_warn_image(prefsw->jack_aplabel);
  } else lives_widget_set_sensitive(prefsw->jack_aplayout, FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->jack_acdef =
    lives_standard_check_button_new(_("Connect using _default server name"),
                                    !(*future_prefs->jack_aserver_cname || (prefs->jack_opts & JACK_INFO_TEMP_NAMES)),
                                    LIVES_BOX(hbox),
                                    H_("The server name will be taken from the environment "
                                       "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                       "set, then the name 'default' will be used instead"));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);
  prefsw->jack_acname =
    lives_standard_entry_new(_("Use _custom server name"), *future_prefs->jack_aserver_cname
                             ? future_prefs->jack_aserver_cname : JACK_DEFAULT_SERVER_NAME,
                             -1, JACK_PARAM_STRING_MAX, LIVES_BOX(hbox), NULL);

  if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) show_warn_image(prefsw->jack_acname, _("Value was set from the commandline"));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_acdef), prefsw->jack_acname, TRUE);

  lives_layout_add_row(LIVES_LAYOUT(layout));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("If connection fails..."), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rb_group = NULL;

  prefsw->jack_acerror =
    lives_standard_radio_button_new(_("Do nothing"), &rb_group, LIVES_BOX(hbox),
                                    H_("With this setting active, LiVES will only ever attempt "
                                       "to connect to an existing jack server, "
                                       "and will never try to start one itself\n"
                                       "If the connection attempt does fail, an error will be generated."));

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget = lives_standard_radio_button_new(_("_Start a jack server"), &rb_group, LIVES_BOX(hbox),
           H_("With this setting active, should the connection attempt fail,\nLiVES will try to start up a jackd server itself,\n"
              "Server values can be set by clicking on the 'Server and Driver Configuration' button."));
  hbox = widget_opts.last_container;

  if (!(future_prefs->jack_opts & JACK_OPTS_START_ASERVER)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->jack_acerror), TRUE);
  } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(widget), TRUE);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;

  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  advbutton =
    lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
        _("Server and _Driver Configuration"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_acerror), advbutton, TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(jack_srv_startup_config), LIVES_INT_TO_POINTER(FALSE));

  layout = prefsw->jack_aplayout2 = lives_layout_new(LIVES_BOX(prefsw->vbox_right_jack));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Audio Options"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_jack_read_autocon =
    lives_standard_check_button_new
    (_("Automatically connect to System Out ports"),
     (future_prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON) ? FALSE : TRUE, LIVES_BOX(hbox),
     H_("Setting this causes LiVES to automatically connect its input client ports to\n"
        "available System Out ports.\nIf not set, then the input ports will need to be connected\n"
        "manually each time LiVES is started with the jack audio player"));

  if (prefs->audio_player != AUD_PLAYER_JACK) lives_widget_set_sensitive(prefsw->jack_aplayout2, FALSE);

  // jack transport
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_jack));

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), hbox,
                       FALSE, FALSE, widget_opts.packing_height);

  tmp = lives_big_and_bold(_("Jack Transport"));
  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new_with_tooltips(tmp, LIVES_BOX(hbox), NULL);
  widget_opts.use_markup = FALSE;
  lives_free(tmp);

#ifndef ENABLE_JACK_TRANSPORT
  show_warn_image(label, _("LiVES must be compiled with jack/transport.h and jack/jack.h\n"
                           "present to use jack transport"));
#else

  prefsw->jack_trans =
    lives_standard_check_button_new(_("ENABLED"), (future_prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT),
                                    LIVES_BOX(hbox),
                                    H_("Allows LiVES playback to be synchronised with other\n"
                                       "jack transport capable applications.\n"
                                       "Can be configured independently of the jack audio player"));

  widget = lives_standard_button_new_with_label(_("Show Client Status"), -1, -1);
  lives_box_pack_start(LIVES_BOX(hbox), widget, FALSE, TRUE, widget_opts.packing_width);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(widget), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(show_jack_status), LIVES_INT_TO_POINTER(1));

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_jack), vbox,
                       FALSE, FALSE, widget_opts.packing_height);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_trans), widget, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_trans), label, FALSE);
  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_trans), vbox, FALSE);

#ifndef ENABLE_JACK_TRANSPORT
  lives_widget_set_active(prefsw->jack_trans, FALSE);
  lives_widget_set_sensitive(prefsw->jack_trans, FALSE);
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

  rb_group = NULL;

  prefsw->jack_srv_dup =
    lives_standard_radio_button_new(_("_Duplicate server settings from audio client"),
                                    &rb_group, LIVES_BOX(hbox), NULL);

  widget =
    lives_standard_radio_button_new(_("_Use separate setings for transport and audio clients (EXPERTS ONLY !)"),
                                    &rb_group, LIVES_BOX(hbox), H_("By enabling this option, it is possible to configure "
                                        "separate jackd servers for audio and transport clients.\n"
                                        "However, care should be taken to ensure that the two server "
                                        "configurations do not conflict with each other."));

  layout = lives_layout_new(LIVES_BOX(vbox));
  if (!prefs->jack_srv_dup) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(widget), TRUE);

  lives_layout_add_label(LIVES_LAYOUT(layout), _("WARNING: creating conflicting settings for audio "
                         "and transport servers may cause problems when starting jackd.\n"
                         "For example, attempting to launch multiple servers connecting to the same soundcard "
                         "may fail with an error condition.\n"
                         "Use of the 'dummy' driver or similar is recommended to avoid this situation."), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->jack_tcdef =
    lives_standard_check_button_new(_("Connect using _default server name"), TRUE, LIVES_BOX(hbox),
                                    H_("The server name will be taken from the environment "
                                       "variable\n$JACK_DEFAULT_SERVER.\nIf that variable is not "
                                       "set, then the name 'default' will be used instead."));

  toggle_sets_active_cond(LIVES_TOGGLE_BUTTON(prefsw->jack_acdef), prefsw->jack_tcdef,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          FALSE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(hbox), WH_LAYOUT_KEY, NULL);

  prefsw->jack_tcname =
    lives_standard_entry_new(_("Use _custom server name"),
                             *future_prefs->jack_tserver_cname ? future_prefs->jack_tserver_cname :
                             JACK_DEFAULT_SERVER_NAME, -1, 1024, LIVES_BOX(hbox), NULL);

  if (prefs->jack_opts & JACK_INFO_TEMP_NAMES) show_warn_image(prefsw->jack_tcname, _("Value was set from the commandline"));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_tcdef), prefsw->jack_tcname, TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->jack_acname), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(copy_entry_text), prefsw->jack_tcname);

  lives_layout_add_row(LIVES_LAYOUT(layout));

  lives_layout_add_label(LIVES_LAYOUT(layout), _("If connection fails..."), TRUE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  rb_group = NULL;

  prefsw->jack_tcerror =
    lives_standard_radio_button_new(_("Do nothing"), &rb_group, LIVES_BOX(hbox),
                                    H_("With this setting active, LiVES will only ever attempt "
                                       "to connect to an existing jack server, "
                                       "and will never try to start one itself\n"
                                       "If the connection attempt does fail, an error will be generated."));

  toggle_sets_active_cond(LIVES_TOGGLE_BUTTON(prefsw->jack_acerror), prefsw->jack_tcerror,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          FALSE);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));

  widget =
    lives_standard_radio_button_new(_("_Start a jack server"), &rb_group, LIVES_BOX(hbox),
                                    H_("With this setting active, should the connection attempt fail,\n"
                                       "LiVES will try to start up a jackd server itself,\n"
                                       "Server values can be set by clicking on the 'Server and Driver Configuration' button."));

  toggle_sets_active_cond(LIVES_TOGGLE_BUTTON(prefsw->jack_acerror), widget,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          (condfuncptr_t)lives_toggle_button_get_active, prefsw->jack_srv_dup,
                          TRUE);

  hbox = widget_opts.last_container;

  if (!(future_prefs->jack_opts & JACK_OPTS_START_TSERVER)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->jack_tcerror), TRUE);
  } else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(widget), TRUE);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  if (!prefs->jack_srv_dup)
    tjack_cfg_exists = jack_get_cfg_file(TRUE, NULL);
  else
    tjack_cfg_exists = ajack_cfg_exists;

  if (!tjack_cfg_exists)
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(prefsw->jack_tcerror), TRUE);

  hbox = widget_opts.last_container;

  advbutton =
    lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
        _("Server and _Driver Configuration"), -1, -1, LIVES_BOX(hbox), TRUE, NULL);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->jack_tcerror), advbutton, TRUE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(advbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(jack_srv_startup_config), LIVES_INT_TO_POINTER(TRUE));

  lives_widget_show_all(layout);
  lives_widget_set_no_show_all(layout, TRUE);
  toggle_sets_visible(LIVES_TOGGLE_BUTTON(prefsw->jack_srv_dup), layout, TRUE);

  layout = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Master options - (in Clip Edit mode, values are taken from Audio playback. "
                         "Setting the audio source to 'External' may be helpful.)"), FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_jack_master =
    lives_standard_check_button_new(_("Transport _Master (playback state)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER)
                                    ? TRUE : FALSE, LIVES_BOX(hbox),
                                    H_("LiVES will trigger transport to start whenever it starts "
                                       "normal playback\nthen subsequently update the transport "
                                       "state if stopped or paused."));

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_master),
                                  LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(after_jack_master_toggled), NULL);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_jack_mtb_start =
    lives_standard_check_button_new(_("Timecode Master (position changes)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART) ?
                                    (lives_toggle_button_get_active
                                     (LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master)))
                                    : FALSE, LIVES_BOX(hbox), H_("LiVES will update the transport time when starting playback, "
                                        "when audio is resynced, and when playback is stopped"));

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_master),
                        prefsw->checkbutton_jack_mtb_start, FALSE);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_mtb_start),
                                  LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(after_jack_upd_toggled), NULL);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_jack_mtb_update =
    lives_standard_check_button_new(_("Full Timebase Master (clock source)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_MASTER) ?
                                    (lives_toggle_button_get_active
                                     (LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_start)))
                                    : FALSE, LIVES_BOX(hbox), H_("During playback, LiVES will continuously update the "
                                        "transport position, acting as Master Clock Source.\n"
                                        "(Ignored if another client is already configured as timebase Master,\n"
                                        "or if playback was not initiated by LiVES)"));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_mtb_start),
                        prefsw->checkbutton_jack_mtb_update, FALSE);

  lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Slave options - (in Clip Edit mode, values are applied to Video playback)"),
                         FALSE);

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_jack_client =
    lives_standard_check_button_new(_("Transport Slave (playback state)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE)
                                    ? TRUE : FALSE, LIVES_BOX(hbox), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_client),
                                  LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_jack_client_toggled), NULL);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_jack_tb_start =
    lives_standard_check_button_new(_("Jack sets start position"),
                                    (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_START) ?
                                    (lives_toggle_button_get_active
                                     (LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client)))
                                    : FALSE, LIVES_BOX(hbox), H_("When playback is triggered from jack transport, "
                                        "the start position will be set\n"
                                        "based on the transport timecode"));

  toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_client),
                        prefsw->checkbutton_jack_tb_start, FALSE);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(prefsw->checkbutton_jack_tb_start),
                                  LIVES_WIDGET_TOGGLED_SIGNAL,
                                  LIVES_GUI_CALLBACK(after_jack_tb_start_toggled), NULL);

  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new("<b>------></b>");
  widget_opts.use_markup = FALSE;
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, FALSE, widget_opts.packing_width);

  hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
  prefsw->checkbutton_jack_tb_client =
    lives_standard_check_button_new(_("Full Timebase Slave (clock source)"),
                                    (future_prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE) ?
                                    (lives_toggle_button_get_active
                                     (LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)))
                                    : FALSE, LIVES_BOX(hbox),
                                    (tmp = H_("During playback, LiVES will continuously monitor the "
                                        "transport position,\nusing it as Master Clock Source"
                                        "for video playback.\nThe audio rate will be dynamically adjusted in "
                                        "order to remain in sync with video\n"
                                        "(This setting is only valid if playback is started via jack transport)")));
  lives_free(tmp);

  lives_widget_set_sensitive(prefsw->checkbutton_jack_tb_client,
                             lives_toggle_button_get_active
                             (LIVES_TOGGLE_BUTTON(prefsw->checkbutton_jack_tb_start)));

  hbox = lives_layout_row_new(LIVES_LAYOUT(layout));

  prefsw->checkbutton_jack_stricts =
    lives_standard_check_button_new(_("Function ONLY as Slave"),
                                    (future_prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)
                                    ? TRUE : FALSE, LIVES_BOX(hbox), H_("Setting this option forces LiVES to ONLY "
                                        "perform the selected actions\nas a response "
                                        "to changes in the transport server\n"
                                        "When set, LiVES cannot act as transport Master"));

  lives_widget_set_sensitive(prefsw->checkbutton_jack_stricts, prefs->audio_player == AUD_PLAYER_JACK);

  tmp = lives_big_and_bold(
          _("(See also Playback -> Audio follows video rate/direction and Audio follows video clip switches)"));
  widget_opts.use_markup = TRUE;
  label = lives_standard_label_new(tmp);
  widget_opts.use_markup = FALSE;
  lives_free(tmp);
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);

#endif

#endif

  pixbuf_jack =
    lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_JACK, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_jack, _("Jack Integration"), LIST_ENTRY_JACK);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_jack);

  // ----------------------,
  // MIDI/js learner       |
  // ----------------------'

  prefsw->vbox_right_midi = lives_vbox_new(FALSE, 0);

  prefsw->scrollw_right_midi = lives_standard_scrolled_window_new(0, 0, prefsw->vbox_right_midi);

  lives_container_set_border_width(LIVES_CONTAINER(prefsw->vbox_right_midi), widget_opts.border_width * 2);

  label = lives_standard_label_new(_("Events to respond to:"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_js = lives_standard_check_button_new(_("_Joystick events"), (prefs->omc_dev_opts & OMC_DEV_JS),
                               LIVES_BOX(hbox), NULL);

  label = lives_standard_label_new(_("Leave blank to use defaults"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->omc_js_entry = lives_standard_fileentry_new((tmp = (_("_Joystick device")))
                         , prefs->omc_js_fname, LIVES_DEVICE_DIR, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                         (tmp2 = (H_("The joystick device, e.g. /dev/input/js0\n"
                                     "Leave blank to use defaults"))));
  lives_free(tmp); lives_free(tmp2);

#ifdef OMC_MIDI_IMPL
  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));
#endif

#endif

#ifdef OMC_MIDI_IMPL
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->checkbutton_omc_midi = lives_standard_check_button_new(_("_MIDI events"), (prefs->omc_dev_opts & OMC_DEV_MIDI),
                                 LIVES_BOX(hbox), NULL);

  add_fill_to_box(LIVES_BOX(hbox));

  if (!(mainw->midi_channel_lock &&
        prefs->midi_rcv_channel > -1)) mchanlist = lives_list_append(mchanlist, (_("All channels")));
  for (i = 0; i < 16; i++) {
    midichan = lives_strdup_printf("%d", i);
    mchanlist = lives_list_append(mchanlist, midichan);
  }

  midichan = lives_strdup_printf("%d", prefs->midi_rcv_channel);

  prefsw->midichan_combo = lives_standard_combo_new(_("MIDI receive _channel"), mchanlist, LIVES_BOX(hbox), NULL);

  lives_list_free_all(&mchanlist);

  if (prefs->midi_rcv_channel > -1) {
    lives_combo_set_active_string(LIVES_COMBO(prefsw->midichan_combo), midichan);
  }

  lives_free(midichan);

  if (mainw->midi_channel_lock && prefs->midi_rcv_channel == -1) lives_widget_set_sensitive(prefsw->midichan_combo, FALSE);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));

  prefsw->midi_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), prefsw->midi_hbox, FALSE, FALSE, widget_opts.packing_height);

#ifdef ALSA_MIDI
  prefsw->alsa_midi = lives_standard_radio_button_new((tmp = (_("Use _ALSA MIDI (recommended)"))), &alsa_midi_group,
                      LIVES_BOX(prefsw->midi_hbox),
                      (tmp2 = (_("Create an ALSA MIDI port which other MIDI devices can be connected to"))));

  lives_free(tmp); lives_free(tmp2);

  prefsw->alsa_midi_dummy = lives_standard_check_button_new((tmp = (_("Create dummy MIDI output"))),
                            prefs->alsa_midi_dummy, LIVES_BOX(prefsw->midi_hbox),
                            (tmp2 = (_("Create a dummy ALSA MIDI output port."))));

  lives_free(tmp); lives_free(tmp2);

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));
#endif

  hbox = lives_hbox_new(FALSE, 0);

#ifdef ALSA_MIDI
  raw_midi_button = lives_standard_radio_button_new((tmp = (_("Use _raw MIDI"))), &alsa_midi_group,
                    LIVES_BOX(hbox), (tmp2 = (_("Read directly from the MIDI device"))));
#endif

  lives_free(tmp); lives_free(tmp2);

#ifdef ALSA_MIDI
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(raw_midi_button), !prefs->use_alsa_midi);
#endif

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);
  prefsw->omc_midi_entry = lives_standard_fileentry_new((tmp = (_("_MIDI device"))), prefs->omc_midi_fname,
                           LIVES_DEVICE_DIR, LONG_ENTRY_WIDTH, PATH_MAX, LIVES_BOX(hbox),
                           (tmp2 = (_("The MIDI device, e.g. /dev/input/midi0"))));

  lives_free(tmp); lives_free(tmp2);

  prefsw->button_midid =
    (LiVESWidget *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(prefsw->omc_midi_entry), BUTTON_KEY);

  label = lives_standard_label_new(_("Advanced"));
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->spinbutton_midicr = lives_standard_spin_button_new((tmp = (_("MIDI check _rate"))),
                              prefs->midi_check_rate, 1., 2000., 1., 10., 0, LIVES_BOX(hbox),
                              (tmp2 = lives_strdup(
                                        _("Number of MIDI checks per keyboard tick. Increasing this may improve "
                                          "MIDI responsiveness, "
                                          "but may slow down playback."))));

  lives_free(tmp); lives_free(tmp2);

  add_fill_to_box(LIVES_BOX(hbox));

  prefsw->spinbutton_midirpt = lives_standard_spin_button_new((tmp = (_("MIDI repeat"))),
                               prefs->midi_rpt, 1., 10000., 10., 100., 0, LIVES_BOX(hbox),
                               (tmp2 = (_("Number of non-reads allowed between successive reads."))));

  lives_free(tmp); lives_free(tmp2);

  label = lives_standard_label_new(_("(Warning: setting this value too high can slow down playback.)"));

  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), label, FALSE, FALSE, widget_opts.packing_height);

#ifdef ALSA_MIDI
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->alsa_midi), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_alsa_midi_toggled), NULL);

  on_alsa_midi_toggled(LIVES_TOGGLE_BUTTON(prefsw->alsa_midi), prefsw);
#endif

  add_hsep_to_box(LIVES_BOX(prefsw->vbox_right_midi));

#endif
#endif

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(prefsw->vbox_right_midi), hbox, FALSE, FALSE, widget_opts.packing_height);

  prefsw->check_midi = lives_standard_check_button_new
                       ((tmp = lives_strdup_printf(_("Midi program synch (requires the files %s and %s)"), EXEC_MIDISTART, EXEC_MIDISTOP)),
                        prefs->midisynch, LIVES_BOX(hbox), NULL);
  lives_free(tmp);

  lives_widget_set_sensitive(prefsw->check_midi, capable->has_midistartstop);

  pixbuf_midi = lives_pixbuf_new_from_stock_at_size(LIVES_LIVES_STOCK_PREF_MIDI, LIVES_ICON_SIZE_CUSTOM, -1);

  prefs_add_to_list(prefsw->prefs_list, pixbuf_midi, _("MIDI/Joystick learner"), LIST_ENTRY_MIDI);
  lives_container_add(LIVES_CONTAINER(dialog_table), prefsw->scrollw_right_midi);

  prefsw->selection = lives_tree_view_get_selection(LIVES_TREE_VIEW(prefsw->prefs_list));
  lives_tree_selection_set_mode(prefsw->selection, LIVES_SELECTION_SINGLE);

  lives_signal_sync_connect(prefsw->selection, LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_prefs_page_changed),
                            (livespointer)prefsw);

  if (!saved_dialog) {
    LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(prefsw->prefs_dialog));
    lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_SPREAD);

    widget_opts.expand |= LIVES_EXPAND_EXTRA_WIDTH;
    // Preferences 'Revert' button
    prefsw->revertbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog),
                           LIVES_STOCK_REVERT_TO_SAVED, NULL, LIVES_RESPONSE_CANCEL);
    lives_widget_show_all(prefsw->revertbutton);

    // Preferences 'Apply' button
    prefsw->applybutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog), LIVES_STOCK_APPLY, NULL,
                          LIVES_RESPONSE_ACCEPT);
    lives_widget_show_all(prefsw->applybutton);

    // Preferences 'Close' button
    prefsw->closebutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(prefsw->prefs_dialog), LIVES_STOCK_CLOSE, NULL,
                          LIVES_RESPONSE_OK);
    lives_widget_show_all(prefsw->closebutton);
    widget_opts.expand = LIVES_EXPAND_DEFAULT;
  } else {
    prefsw->revertbutton = saved_revertbutton;
    prefsw->applybutton = saved_applybutton;
    prefsw->closebutton = saved_closebutton;
    lives_widget_set_sensitive(prefsw->closebutton, TRUE);
  }
  lives_button_grab_default_special(prefsw->closebutton);

  prefs->cb_is_switch = FALSE;

  lives_widget_add_accelerator(prefsw->closebutton, LIVES_WIDGET_CLICKED_SIGNAL, prefsw->accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);

  // Set 'Revert' button as inactive since there are no changes yet
  lives_widget_set_sensitive(prefsw->revertbutton, FALSE);
  // Set 'Apply' button as inactive since there are no changes yet
  lives_widget_set_sensitive(prefsw->applybutton, FALSE);

  // Connect signals for 'Apply' button activity handling
  if (prefsw->theme_style2)
    ACTIVE(theme_style2, TOGGLED);

  ACTIVE(theme_style3, TOGGLED);
  ACTIVE(theme_style4, TOGGLED);

  ACTIVE(cmdline_entry, CHANGED);

  ACTIVE(wpp_entry, CHANGED);
  ACTIVE(frei0r_entry, CHANGED);
  ACTIVE(libvis_entry, CHANGED);
  ACTIVE(ladspa_entry, CHANGED);

  ACTIVE(fs_max_check, TOGGLED);
  ACTIVE(recent_check, TOGGLED);
  ACTIVE(stop_screensaver_check, TOGGLED);
  ACTIVE(open_maximised_check, TOGGLED);
  ACTIVE(mouse_scroll, TOGGLED);
  ACTIVE(checkbutton_ce_maxspect, TOGGLED);
  ACTIVE(ce_thumbs, TOGGLED);
  ACTIVE(checkbutton_button_icons, TOGGLED);
  if (prefsw->checkbutton_menu_icons)
    ACTIVE(checkbutton_menu_icons, TOGGLED);
  ACTIVE(checkbutton_extra_colours, TOGGLED);
  ACTIVE(checkbutton_hfbwnp, TOGGLED);
  ACTIVE(checkbutton_show_asrc, TOGGLED);
  ACTIVE(checkbutton_show_ttips, TOGGLED);
  ACTIVE(rb_startup_mt, TOGGLED);
  ACTIVE(rb_startup_ce, TOGGLED);

  ACTIVE(spinbutton_warn_ds, VALUE_CHANGED);
  ACTIVE(spinbutton_crit_ds, VALUE_CHANGED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_warn_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(spinbutton_ds_value_changed), LIVES_INT_TO_POINTER(FALSE));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_crit_ds), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(spinbutton_ds_value_changed), LIVES_INT_TO_POINTER(TRUE));

  ACTIVE(spinbutton_gmoni, VALUE_CHANGED);
  ACTIVE(spinbutton_pmoni, VALUE_CHANGED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_gmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(pmoni_gmoni_changed), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->spinbutton_pmoni), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(pmoni_gmoni_changed), NULL);

  ACTIVE(forcesmon, TOGGLED);
  ACTIVE(checkbutton_stream_audio, TOGGLED);
  ACTIVE(checkbutton_rec_after_pb, TOGGLED);
  ACTIVE(spinbutton_warn_ds, VALUE_CHANGED);
  ACTIVE(spinbutton_crit_ds, VALUE_CHANGED);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mt_enter_defs), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);

  ACTIVE(mt_enter_prompt, TOGGLED);
  ACTIVE(checkbutton_render_prompt, TOGGLED);

  ACTIVE(spinbutton_mt_def_width, VALUE_CHANGED);
  ACTIVE(spinbutton_mt_def_height, VALUE_CHANGED);
  ACTIVE(spinbutton_mt_def_fps, VALUE_CHANGED);

  ACTIVE(backaudio_checkbutton, TOGGLED);
  ACTIVE(pertrack_checkbutton, TOGGLED);

  ACTIVE(spinbutton_mt_undo_buf, VALUE_CHANGED);

  ACTIVE(checkbutton_mt_exit_render, TOGGLED);

  ACTIVE(spinbutton_mt_ab_time, VALUE_CHANGED);
  ACTIVE(spinbutton_max_disp_vtracks, VALUE_CHANGED);

  ACTIVE(mt_autoback_always, TOGGLED);
  ACTIVE(mt_autoback_never, TOGGLED);
  ACTIVE(mt_autoback_every, TOGGLED);

  ACTIVE(video_open_entry, CHANGED);
  ACTIVE(frameblank_entry, CHANGED);
  ACTIVE(sepimg_entry, CHANGED);

  ACTIVE(spinbutton_ocp, VALUE_CHANGED);
  ACTIVE(jpeg, TOGGLED);

  ACTIVE(checkbutton_instant_open, TOGGLED);
  ACTIVE(checkbutton_auto_deint, TOGGLED);
  ACTIVE(checkbutton_auto_trim, TOGGLED);
  ACTIVE(checkbutton_concat_images, TOGGLED);
  ACTIVE(checkbutton_lb, TOGGLED);
  ACTIVE(checkbutton_lbmt, TOGGLED);
  ACTIVE(checkbutton_lbenc, TOGGLED);
  ACTIVE(no_lb_gens, TOGGLED);
  ACTIVE(checkbutton_screengamma, TOGGLED);
  ACTIVE(spinbutton_gamma, VALUE_CHANGED);
  ACTIVE(pbq_adaptive, TOGGLED);

  ACTIVE(pbq_combo, CHANGED);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(pp_combo), LIVES_WIDGET_CHANGED_SIGNAL,
                            LIVES_GUI_CALLBACK(apply_button_set_enabled), NULL);
  ACTIVE(audp_combo, CHANGED);

  ACTIVE(checkbutton_show_stats, TOGGLED);

  ACTIVE(checkbutton_afollow, TOGGLED);
  ACTIVE(checkbutton_aclips, TOGGLED);
  ACTIVE(resync_fps, TOGGLED);
  ACTIVE(resync_vpos, TOGGLED);
  ACTIVE(resync_adir, TOGGLED);
  ACTIVE(resync_aclip, TOGGLED);

  ACTIVE(afreeze_lock, TOGGLED);
  ACTIVE(afreeze_ping, TOGGLED);
  ACTIVE(afreeze_sync, TOGGLED);
  ACTIVE(alock_reset, TOGGLED);
  ACTIVE(auto_unlock, TOGGLED);

  ACTIVE(rdesk_audio, TOGGLED);

  ACTIVE(rframes, TOGGLED);
  ACTIVE(rfps, TOGGLED);
  ACTIVE(reffects, TOGGLED);
  ACTIVE(rclips, TOGGLED);
  ACTIVE(raudio, TOGGLED);
  ACTIVE(raudio_alock, TOGGLED);
  ACTIVE(rextaudio, TOGGLED);

  ACTIVE(pa_gens, TOGGLED);

  ACTIVE(spinbutton_ext_aud_thresh, VALUE_CHANGED);

  ACTIVE(encoder_combo, CHANGED);

  if (ARE_PRESENT(encoder_plugins)) {
    ACTIVE(ofmt_combo, CHANGED);
    ACTIVE(acodec_combo, CHANGED);
  }

  ACTIVE(checkbutton_antialias, TOGGLED);
  ACTIVE(checkbutton_load_rfx, TOGGLED);
  ACTIVE(checkbutton_apply_gamma, TOGGLED);

  ACTIVE(spinbutton_rte_keys, VALUE_CHANGED);
  ACTIVE(spinbutton_rte_modes, VALUE_CHANGED);
  ACTIVE(spinbutton_nfx_threads, VALUE_CHANGED);

  ACTIVE(checkbutton_threads, TOGGLED);

  ACTIVE(checkbutton_warn_fps, TOGGLED);
  ACTIVE(checkbutton_warn_fsize, TOGGLED);
  ACTIVE(checkbutton_warn_save_set, TOGGLED);
  ACTIVE(checkbutton_warn_mplayer, TOGGLED);
  ACTIVE(checkbutton_warn_missplugs, TOGGLED);
  ACTIVE(checkbutton_warn_prefix, TOGGLED);
  ACTIVE(checkbutton_warn_dup_set, TOGGLED);
  ACTIVE(checkbutton_warn_layout_clips, TOGGLED);
  ACTIVE(checkbutton_warn_layout_close, TOGGLED);
  ACTIVE(checkbutton_warn_layout_delete, TOGGLED);
  ACTIVE(checkbutton_warn_layout_shift, TOGGLED);
  ACTIVE(checkbutton_warn_layout_alter, TOGGLED);
  ACTIVE(checkbutton_warn_layout_adel, TOGGLED);
  ACTIVE(checkbutton_warn_layout_ashift, TOGGLED);
  ACTIVE(checkbutton_warn_layout_aalt, TOGGLED);
  ACTIVE(checkbutton_warn_layout_popup, TOGGLED);
  ACTIVE(checkbutton_warn_layout_reload, TOGGLED);
  ACTIVE(checkbutton_warn_discard_layout, TOGGLED);
  ACTIVE(checkbutton_warn_mt_achans, TOGGLED);
  ACTIVE(checkbutton_warn_mt_no_jack, TOGGLED);
  ACTIVE(checkbutton_warn_dis_dec, TOGGLED);

  ACTIVE(spinbutton_warn_fsize, VALUE_CHANGED);

#ifdef HAVE_LDVGRAB
  ACTIVE(checkbutton_warn_after_dvgrab, TOGGLED);
#endif
#ifdef HAVE_YUV4MPEG
  ACTIVE(checkbutton_warn_yuv4m_open, TOGGLED);
#endif
  ACTIVE(checkbutton_warn_layout_gamma, TOGGLED);
  ACTIVE(checkbutton_warn_layout_lb, TOGGLED);
  ACTIVE(checkbutton_warn_layout_wipe, TOGGLED);
  ACTIVE(checkbutton_warn_no_pulse, TOGGLED);
  ACTIVE(checkbutton_warn_after_crash, TOGGLED);
#ifdef ENABLE_JACK
  ACTIVE(checkbutton_warn_jack_scrpts, TOGGLED);
#endif
  ACTIVE(checkbutton_warn_dmgd_audio, TOGGLED);
  ACTIVE(checkbutton_warn_mt_backup_space, TOGGLED);
  ACTIVE(checkbutton_warn_vjmode_enter, TOGGLED);

  ACTIVE(check_midi, TOGGLED);
  ACTIVE(midichan_combo, CHANGED);

  ACTIVE(ins_speed, TOGGLED);

  ACTIVE(cdplay_entry, CHANGED);

  ACTIVE(spinbutton_def_fps, VALUE_CHANGED);

  ACTIVE(theme_combo, CHANGED);

  ACTIVE(spinbutton_bwidth, VALUE_CHANGED);
#ifdef ENABLE_OSC
  ACTIVE(spinbutton_osc_udp, VALUE_CHANGED);
  ACTIVE(enable_OSC_start, TOGGLED);
  ACTIVE(enable_OSC, TOGGLED);
#endif

#ifdef ENABLE_JACK_TRANSPORT
  ACTIVE(jack_trans, TOGGLED);
  ACTIVE(jack_tcname, CHANGED);
  ACTIVE(jack_tcdef, TOGGLED);
  ACTIVE(jack_tcerror, TOGGLED);
  ACTIVE(checkbutton_jack_master, TOGGLED);
  ACTIVE(checkbutton_jack_client, TOGGLED);
  ACTIVE(checkbutton_jack_tb_start, TOGGLED);
  ACTIVE(checkbutton_jack_mtb_start, TOGGLED);
  ACTIVE(checkbutton_jack_mtb_update, TOGGLED);
  ACTIVE(checkbutton_jack_tb_client, TOGGLED);
  ACTIVE(checkbutton_jack_stricts, TOGGLED);
#endif

#ifdef ENABLE_JACK
  ACTIVE(jack_acname, CHANGED);
  ACTIVE(jack_acdef, TOGGLED);
  ACTIVE(jack_acerror, TOGGLED);
  ACTIVE(checkbutton_jack_read_autocon, TOGGLED);
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  ACTIVE(checkbutton_omc_js, TOGGLED);
  ACTIVE(omc_js_entry, CHANGED);
#endif
#ifdef OMC_MIDI_IMPL
  ACTIVE(checkbutton_omc_midi, TOGGLED);
#ifdef ALSA_MIDI
  ACTIVE(alsa_midi, TOGGLED);
  ACTIVE(alsa_midi_dummy, TOGGLED);
#endif
  ACTIVE(omc_midi_entry, CHANGED);
  ACTIVE(spinbutton_midicr, VALUE_CHANGED);
  ACTIVE(spinbutton_midirpt, VALUE_CHANGED);
#endif
#endif

  if (ARE_PRESENT(encoder_plugins)) {
    prefsw->encoder_name_fn = lives_signal_sync_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->encoder_combo)),
                              LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_encoder_entry_changed), NULL);

    prefsw->encoder_ofmt_fn = lives_signal_sync_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->ofmt_combo)),
                              LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_encoder_ofmt_changed), NULL);
  }

  prefsw->audp_entry_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(LIVES_COMBO(prefsw->audp_combo)),
                            LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_audp_entry_changed), NULL);

#ifdef ENABLE_OSC
  lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->enable_OSC), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_osc_enable_toggled),
                            (livespointer)prefsw->enable_OSC_start);
#endif
  if (saved_dialog == NULL) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->revertbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_prefs_revert_clicked), NULL);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->prefs_dialog), LIVES_WIDGET_DELETE_EVENT,
                              LIVES_GUI_CALLBACK(on_prefs_close_clicked), NULL);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->applybutton), LIVES_WIDGET_CLICKED_SIGNAL,
                              LIVES_GUI_CALLBACK(on_prefs_apply_clicked), NULL);
  }

  prefsw->close_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(prefsw->closebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prefs_close_clicked), NULL);

  lives_list_free_all(&audp);

  if (prefs_current_page == -1) {
    if (!mainw->multitrack)
      select_pref_list_row(LIST_ENTRY_GUI, prefsw);
    else {
      sel_last = TRUE;
      select_pref_list_row(LIST_ENTRY_MULTITRACK, prefsw);
    }
  } else select_pref_list_row(prefs_current_page, prefsw);

  lives_widget_set_opacity(prefsw->prefs_dialog, 0.);
  lives_widget_show_all(prefsw->prefs_dialog);

  on_prefs_page_changed(prefsw->selection, prefsw);
  callibrate_paned(LIVES_PANED(prefsw->dialog_hpaned),
                   lives_scrolled_window_get_hscrollbar(LIVES_SCROLLED_WINDOW(prefsw->list_scroll)));
  lives_widget_set_opacity(prefsw->prefs_dialog, 1.);

  lives_widget_show_all(prefsw->prefs_dialog);
  lives_widget_process_updates(prefsw->prefs_dialog);

  if (sel_last)
    lives_scrolled_window_scroll_to(LIVES_SCROLLED_WINDOW(prefsw->list_scroll), LIVES_POS_BOTTOM);

  lives_widget_show_all(prefsw->prefs_dialog);
  lives_widget_queue_draw_and_update(prefsw->prefs_dialog);

  return prefsw;
}


void on_preferences_activate(LiVESMenuItem * menuitem, livespointer user_data) {
  LiVESWidget *saved_dialog = (LiVESWidget *)user_data;
  mt_needs_idlefunc = FALSE;

  if (mainw->multitrack) {
    if (mainw->multitrack->idlefunc > 0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc = 0;
      mt_needs_idlefunc = TRUE;
    }
    mt_desensitise(mainw->multitrack);
  }

  if (menuitem) prefs_current_page = -1;

  if (prefsw && prefsw->prefs_dialog) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
    return;
  }

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_context_update();

  prefsw = create_prefs_dialog(saved_dialog);
  lives_widget_show(prefsw->prefs_dialog);
  lives_window_set_position(LIVES_WINDOW(prefsw->prefs_dialog), LIVES_WIN_POS_CENTER_ALWAYS);
  lives_widget_queue_draw(prefsw->prefs_dialog);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, prefsw->prefs_dialog);
}


/*!
  Closes preferences dialog window
*/
void on_prefs_close_clicked(LiVESButton * button, livespointer user_data) {
  lives_list_free_all(&prefs->acodec_list);
  if (prefsw) {
    invalidate_pref_widgets(prefsw->prefs_dialog);
    lives_list_free_all(&prefsw->pbq_list);
    lives_tree_view_set_model(LIVES_TREE_VIEW(prefsw->prefs_list), NULL);
    lives_free(prefsw->audp_name);
    lives_free(prefsw->orig_audp_name);
    lives_freep((void **)&resaudw);
    if (future_prefs->disabled_decoders) {
      lives_list_free(future_prefs->disabled_decoders);
      future_prefs->disabled_decoders = NULL;
    }
    lives_general_button_clicked(button, prefsw);
    prefsw = NULL;
  }
  if (mainw->prefs_need_restart) {
    do_shutdown_msg();
    on_quit_activate(NULL, NULL);
  }
  if (mainw->multitrack) {
    mt_sensitise(mainw->multitrack);
    if (mt_needs_idlefunc) {
      mainw->multitrack->idlefunc = mt_idle_add(mainw->multitrack);
    }
  }
}


void pref_change_images(void) {
  if (prefs->show_gui) {
    if (mainw->current_file == -1) {
      load_start_image(0);
      load_end_image(0);
      if (mainw->preview_box) load_preview_image(FALSE);
    }
    lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    if (mainw->multitrack && mainw->multitrack->sep_image) {
      lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->multitrack->sep_image), mainw->imsep);
      mt_show_current_frame(mainw->multitrack, FALSE);
      lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    }
  }
}


void pref_change_xcolours(void) {
  // minor colours changed
  if (prefs->show_gui) {
    if (mainw->multitrack) {
      resize_timeline(mainw->multitrack);
      set_mt_colours(mainw->multitrack);
    } else {
      update_play_times();
      lives_widget_queue_draw(LIVES_MAIN_WINDOW_WIDGET);
    }
  }
}


void pref_change_colours(void) {
  if (mainw->preview_box) {
    set_preview_box_colours();
  }

  if (prefs->show_gui) {
    set_colours(&palette->normal_fore, &palette->normal_back, &palette->menu_and_bars_fore, &palette->menu_and_bars, \
                &palette->info_base, &palette->info_text);

    if (mainw->multitrack) {
      set_mt_colours(mainw->multitrack);
      scroll_tracks(mainw->multitrack, mainw->multitrack->top_track, FALSE);
      track_select(mainw->multitrack);
      mt_clip_select(mainw->multitrack, FALSE);
    } else update_play_times();
  }
}


void on_prefs_apply_clicked(LiVESButton * button, livespointer user_data) {
  boolean needs_restart = FALSE;

  if (mainw->prefs_changed & PREFS_NEEDS_REVERT) {
    on_prefs_revert_clicked(button, NULL);
    goto done;
  }
  lives_set_cursor_style(LIVES_CURSOR_BUSY, prefsw->prefs_dialog);

  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->applybutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->revertbutton), FALSE);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), FALSE);

  // Apply preferences
  needs_restart = apply_prefs(FALSE);

  if (!mainw->prefs_need_restart) {
    mainw->prefs_need_restart = needs_restart;
  }

  if (needs_restart) {
    //do_info_dialog(_("For the directory change to take effect LiVES will restart when preferences dialog closes."));
    do_info_dialog(_("LiVES will restart when preferences dialog closes."));
  }

  if (mainw->prefs_changed & PREFS_RTE_KEYMODES_CHANGED) {
    refresh_rte_window();
  }

  if ((mainw->prefs_changed & PREFS_THEME_CHANGED) || (mainw->prefs_changed & PREFS_THEME_MINOR_CHANGE)) {
    if ((mainw->prefs_changed & PREFS_THEME_CHANGED) && !lives_strcmp(future_prefs->theme, LIVES_THEME_NONE)) {
      lives_widget_set_sensitive(mainw->export_theme, FALSE);
      do_info_dialog(_("Disabling the theme will not take effect until the next time you start LiVES."));
    } else {
      do_info_dialog(_("Some theme changes will only take full effect after restarting LiVES."));
      if (mainw->prefs_changed & PREFS_THEME_CHANGED) {
        set_double_pref(PREF_CPICK_VAR, DEF_CPICK_VAR);
        set_double_pref(PREF_CPICK_TIME, (prefs->cptime = -DEF_CPICK_TIME));
      }
    }
  } else
    lives_widget_set_sensitive(mainw->export_theme, TRUE);

  if (mainw->prefs_changed & PREFS_JACK_CHANGED) {
    do_info_dialog(_("Some jack options require a restart of LiVES to take effect."));
  }

  if (!(mainw->prefs_changed & PREFS_THEME_CHANGED) &&
      ((mainw->prefs_changed & PREFS_IMAGES_CHANGED) ||
       (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED) ||
       (mainw->prefs_changed & PREFS_COLOURS_CHANGED))) {
    // set details in prefs
    set_palette_prefs(TRUE);
    if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
      load_theme_images();
    }
  }

  if (mainw->prefs_changed & PREFS_IMAGES_CHANGED) {
    pref_change_images();
  }

  if (mainw->prefs_changed & PREFS_XCOLOURS_CHANGED) {
    pref_change_xcolours();
  }

  if (mainw->prefs_changed & PREFS_COLOURS_CHANGED) {
    // major coulours changed
    // force reshow of window
    pref_change_colours();
    on_prefs_revert_clicked(button, NULL);
  } else if (mainw->prefs_changed & PREFS_NEEDS_REVERT) {
    on_prefs_revert_clicked(button, NULL);
  }

done:
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  lives_set_cursor_style(LIVES_CURSOR_NORMAL, prefsw->prefs_dialog);

  lives_button_grab_default_special(prefsw->closebutton);
  lives_widget_set_sensitive(LIVES_WIDGET(prefsw->closebutton), TRUE);

  mainw->prefs_changed = 0;
}


/*
  Function is used to select particular row in preferences selection list
  selection is performed according to provided index which is one of LIST_ENTRY_* constants
*/
static void select_pref_list_row(uint32_t selected_idx, _prefsw * prefsw) {
  LiVESTreeIter iter;
  LiVESTreeModel *model;
  boolean valid;
  uint32_t idx;

  model = lives_tree_view_get_model(LIVES_TREE_VIEW(prefsw->prefs_list));
  valid = lives_tree_model_get_iter_first(model, &iter);
  while (valid) {
    lives_tree_model_get(model, &iter, LIST_NUM, &idx, -1);
    //
    if (idx == selected_idx) {
      lives_tree_selection_select_iter(prefsw->selection, &iter);
      lives_tree_model_get(model, &iter, LIST_NUM, &idx, -1);
      break;
    }
    //
    valid = lives_tree_model_iter_next(model, &iter);
  }
}


void on_prefs_revert_clicked(LiVESButton * button, livespointer user_data) {
  LiVESWidget *saved_dialog;
  int i;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(prefsw->prefs_dialog);

  if (future_prefs->vpp_argv) {
    for (i = 0; future_prefs->vpp_argv[i]; lives_free(future_prefs->vpp_argv[i++]));

    lives_free(future_prefs->vpp_argv);

    future_prefs->vpp_argv = NULL;
  }
  memset(future_prefs->vpp_name, 0, 64);

  lives_list_free_all(&prefs->acodec_list);
  lives_list_free_all(&prefsw->pbq_list);
  lives_tree_view_set_model(LIVES_TREE_VIEW(prefsw->prefs_list), NULL);

  lives_free(prefsw->audp_name);
  lives_free(prefsw->orig_audp_name);

  if (future_prefs->disabled_decoders) {
    lives_list_free(future_prefs->disabled_decoders);
    future_prefs->disabled_decoders = NULL;
  }

  saved_dialog = prefsw->prefs_dialog;
  saved_revertbutton = prefsw->revertbutton;
  saved_applybutton = prefsw->applybutton;
  saved_closebutton = prefsw->closebutton;
  lives_signal_handler_disconnect(prefsw->closebutton, prefsw->close_func);
  lives_widget_remove_accelerator(prefsw->closebutton, prefsw->accel_group, LIVES_KEY_Escape, (LiVESXModifierType)0);

  lives_widget_destroy(prefsw->dialog_hpaned);
  lives_freep((void **)&prefsw);

#ifdef ENABLE_JACK
  future_prefs->jack_opts = prefs->jack_opts;
  lives_snprintf(future_prefs->jack_aserver_cfg, PATH_MAX, "%s", prefs->jack_aserver_cfg);
  lives_snprintf(future_prefs->jack_aserver_cname, PATH_MAX, "%s", prefs->jack_aserver_cname);
  lives_snprintf(future_prefs->jack_aserver_sname, PATH_MAX, "%s", prefs->jack_aserver_sname);
  lives_snprintf(future_prefs->jack_tserver_cfg, PATH_MAX, "%s", prefs->jack_tserver_cfg);
  lives_snprintf(future_prefs->jack_tserver_cname, PATH_MAX, "%s", prefs->jack_tserver_cname);
  lives_snprintf(future_prefs->jack_tserver_sname, PATH_MAX, "%s", prefs->jack_tserver_sname);
#endif

  if (future_prefs->def_fontstring) {
    if (!capable->def_fontstring) {
      pref_factory_utf8(PREF_INTERFACE_FONT, future_prefs->def_fontstring, FALSE);
    }
    if (future_prefs->def_fontstring != capable->def_fontstring) lives_free(future_prefs->def_fontstring);
    future_prefs->def_fontstring = NULL;
  }

  on_preferences_activate(NULL, saved_dialog);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
}


static int text_to_lives_perm(const char *text) {
  if (!text || !*text) return LIVES_PERM_INVALID;
  if (!strcmp(text, "DOWNLOADLOCAL")) return LIVES_PERM_DOWNLOAD_LOCAL;
  if (!strcmp(text, "COPYLOCAL")) return LIVES_PERM_COPY_LOCAL;
  return LIVES_PERM_INVALID;
}

boolean lives_ask_permission(char **argv, int argc, int offs) {
  const char *sudocom = NULL;
  char *msg;
  boolean ret;
  int what = atoi(argv[offs]);
  if (what == LIVES_PERM_INVALID && *argv[offs]) {
    what = text_to_lives_perm(argv[offs]);
  }

  switch (what) {
  case LIVES_PERM_OSC_PORTS:
    return ask_permission_dialog(what);
  case LIVES_PERM_DOWNLOAD_LOCAL:
  case LIVES_PERM_COPY_LOCAL:
    if (argc >= 5 && strstr(argv[4], "_TRY_SUDO_")) sudocom = (const char *)argv[2];
    ret = ask_permission_dialog_complex(what, argv, argc, ++offs, sudocom);
    return ret;
  default:
    msg = lives_strdup_printf("Unknown permission (%d) requested", what);
    LIVES_WARN(msg);
    lives_free(msg);
  }
  return FALSE;
}

#if 0
void optimize(void) {
  // TODO
  LiVESWidget *dialog = lives_standard_dialog_new(_("Optimisations"), TRUE, -1, -1);
  LiVESWidget *vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *vbox2 = lives_vbox_new(FALSE, 0);
  int width = -1, height = -1;
  LiVESWidget *scrolledwindow = lives_standard_scrolled_window_new(width, height, vbox2);
  LiVESWidget *cb;
  LiVESResponseType resp;

  lives_box_pack_start(LIVES_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);
  cb = lives_standard_check_button_new(_("_Use openGL playback plugin"), mainw->vpp != NULL,
                                       LIVES_BOX(vbox2), NULL);

  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  if (resp == LIVES_RESPONSE_OK) lives_widget_destroy(dialog);
}
#endif
