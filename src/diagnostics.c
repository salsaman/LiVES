// diagnostics.h
// LiVES
// (c) G. Finch 2003 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#include "diagnostics.h"

ticks_t timerinfo;

static void show_timer_info(void) {
  g_print("\n\nTest completed in %.4f seconds\n\n",
          ((double)lives_get_current_ticks() - (double)timerinfo) / TICKS_PER_SECOND_DBL);
  timerinfo = lives_get_current_ticks();
}

#ifdef WEED_STARTUP_TEST

int run_weed_startup_tests(void) {
  weed_plant_t *plant;
  int a, type, ne, st, flags;
  int *intpr;
  char *str;
  int pint[4];//, zint[4];
  weed_error_t werr;
  char **keys;
  void *ptr, *ptr2;
  void *ptra[4];
  char *s[4];
  int n;
  weed_size_t nleaves;

  g_print("Testing libweed functionality:\n\n");

  timerinfo = lives_get_current_ticks();

  // run some tests..
  plant = weed_plant_new(WEED_PLANT_HOST_INFO);
  fprintf(stderr, "plant is %p\n", plant);

  type = weed_get_int_value(plant, WEED_LEAF_TYPE, &werr);
  fprintf(stderr, "type is %d, err was %d\n", type, werr);

  ne = weed_leaf_num_elements(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "ne was %d\n", ne);

  st = weed_leaf_seed_type(plant, "type");
  fprintf(stderr, "seedtype is %d\n", st);

  flags = weed_leaf_get_flags(plant, WEED_LEAF_TYPE);
  fprintf(stderr, "flags is %d\n", flags);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  //
  fprintf(stderr, "check NULL plant\n");
  type = weed_get_int_value(NULL, WEED_LEAF_TYPE, &werr);

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  ne = weed_leaf_num_elements(NULL, WEED_LEAF_TYPE);

  fprintf(stderr, "ne was %d\n", ne);
  st = weed_leaf_seed_type(NULL, "type");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = weed_leaf_get_flags(NULL, WEED_LEAF_TYPE);

  fprintf(stderr, "flags is %d\n", flags);

  keys = weed_plant_list_leaves(NULL, NULL);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  //
  fprintf(stderr, "Check NULL key \n");

  type = weed_get_int_value(plant, NULL, &werr);
  fprintf(stderr, "type is %d, err was %d\n", type, werr);

  ne = weed_leaf_num_elements(plant, NULL);
  fprintf(stderr, "ne was %d\n", ne);

  st = weed_leaf_seed_type(plant, NULL);
  fprintf(stderr, "seedtype is %d\n", st);

  flags = weed_leaf_get_flags(plant, NULL);
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "Check zero key \n");
  type = weed_get_int_value(plant, "", &werr);

  fprintf(stderr, "type is %d, err was %d\n", type, werr);
  ne = weed_leaf_num_elements(plant, "");

  fprintf(stderr, "ne was %d\n", ne);
  st = weed_leaf_seed_type(plant, "");

  fprintf(stderr, "seedtype is %d\n", st);
  flags = weed_leaf_get_flags(plant, "");
  fprintf(stderr, "flags is %d\n", flags);

  fprintf(stderr, "checking get / set values\n");

  weed_set_int_value(plant, "Test", 99);
  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value read was %d, err was %d\n", a, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_int_value(plant, "Test", 143);
  a = weed_get_int_value(plant, "Test", &werr);

  fprintf(stderr, "value read was %d, err was %d\n", a, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "abc");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "12345");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", NULL);
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test3", NULL);
  str = weed_get_string_value(plant, "Test3", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, NULL, NULL);
  str = weed_get_string_value(NULL, NULL, &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  pint[0] = 10000000;
  pint[1] = 1;
  pint[2] = 5;
  pint[3] = -199;

  werr = weed_set_int_array(plant, "intarray", 4, pint);
  fprintf(stderr, "int array set, err was %d\n", werr);

  intpr = weed_get_int_array(plant, "intarray", &werr);
  fprintf(stderr, "int array got %d %d %d %d , err was %d\n", intpr[0], intpr[1], intpr[2], intpr[3], werr);

  intpr = weed_get_int_array(plant, "xintarray", &werr);
  fprintf(stderr, "int array got %p, err was %d\n", intpr, werr);

  intpr = weed_get_int_array(NULL, "xintarray",  &werr);
  fprintf(stderr, "int array got %p , err was %d\n", intpr, werr);

  fprintf(stderr, "flag tests\n");

  werr = weed_set_int_value(plant, "type", 99);
  fprintf(stderr, "set type returned %d\n", werr);

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags are %d\n", flags);

  werr = weed_leaf_set_flags(plant, "type", 0);
  fprintf(stderr, "type setflags %d\n", werr);

  werr = weed_set_int_value(plant, "type", 123);
  fprintf(stderr, "set type returned %d\n", werr);

  a = weed_get_int_value(plant, "type", &werr);
  fprintf(stderr, "get type returned %d %d\n", a, werr);

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags are %d\n", flags);

  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  fprintf(stderr, "type setflags %d\n", werr);

  werr = weed_set_int_value(plant, "type", 200);
  fprintf(stderr, "set type returned %d\n", werr);

  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "type flags are %d\n", flags);

  flags = weed_leaf_get_flags(plant, "Test2");
  fprintf(stderr, "test getflags %d\n", flags);

  weed_set_string_value(plant, "Test2", "abcde");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  weed_set_string_value(plant, "Test2", "888888");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  weed_leaf_set_flags(plant, "Test2", WEED_FLAG_IMMUTABLE);

  werr = weed_set_string_value(plant, "Test2", "hello");
  fprintf(stderr, "set immutable returned %d\n", werr);

  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  weed_leaf_set_flags(plant, "Test2", 0);

  weed_set_string_value(plant, "Test2", "OK");
  str = weed_get_string_value(plant, "Test2", &werr);

  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  weed_set_string_value(plant, "string1", "abccc");
  weed_set_string_value(plant, "string2", "xyyyyzz");
  weed_set_string_value(plant, "string3", "11111  11111");

  werr = weed_set_string_value(plant, "string2", "xxxxx");
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "value read was %s, err was %d\n", str, werr);

  werr = weed_leaf_delete(plant, "string1");
  fprintf(stderr, "del leaf returned %d\n", werr);

  str = weed_get_string_value(plant, "string1", &werr);
  fprintf(stderr, "del leaf returned %s %d\n", str, werr);

  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);
  weed_leaf_set_flags(plant, "string2", WEED_FLAG_UNDELETABLE);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_delete(plant, "string2");
  fprintf(stderr, "del leaf returned %d\n", werr);
  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "del leaf returned %s %d\n", str, werr);
  weed_leaf_set_flags(plant, "string2", 0);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);
  werr = weed_leaf_delete(plant, "string2");
  fprintf(stderr, "del leaf returned %d\n", werr);

  str = weed_get_string_value(plant, "string2", &werr);
  fprintf(stderr, "del leaf val returned %s %d\n", str, werr);

  werr = weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);
  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  //

  werr = weed_set_voidptr_value(plant, "nullptr",  NULL);
  fprintf(stderr, "set null void * returned %d\n", werr);

  ptr = weed_get_voidptr_value(plant, "nullptr", &werr);
  fprintf(stderr, "get null vooid * returned (%p) %d\n", ptr, werr);

  ptr = weed_get_voidptr_value(plant, "nullptrxx", &werr);
  fprintf(stderr, "get nonexist void * returned (%p) %d\n", ptr, werr);

  werr = weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 0, NULL);
  fprintf(stderr, "set null basic voidptr zero returned %d\n", werr);

  ptr = weed_get_voidptr_value(plant, "nullbasic", &werr);
  fprintf(stderr, "get null basic voidptr 0 returned (%p) %d\n", ptr, werr);

  /* werr = weed_leaf_set(plant, "nullbasic", WEED_SEED_VOIDPTR, 1, NULL); */
  /* fprintf(stderr, "set null string returned %d\n", werr); */

  /* ptr = weed_get_voidptr_value(plant, "nullbasic", &werr); */
  /* fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr); */

  ptr2 = NULL;
  werr = weed_leaf_set(plant, "indirect", WEED_SEED_VOIDPTR, 1, &ptr2);
  fprintf(stderr, "set null ptr returned %d\n", werr);

  ptr = weed_get_voidptr_value(plant, "indirect", &werr);
  fprintf(stderr, "get null string returned (%p) %d\n", ptr, werr);

  ptra[0] = &werr;
  ptra[1] = &keys;
  ptra[2] = NULL;
  ptra[3] = &ptra[3];

  weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, &ptra);
  fprintf(stderr, "set null array elem ptra returned %d\n", werr);

  void **ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], werr);

  s[0] = "okok";
  s[1] = "1ok2ok";
  s[2] = NULL;
  s[3] = "1ok2ok";

  weed_leaf_set(plant, "ptrs", WEED_SEED_VOIDPTR, 4, &ptra);
  fprintf(stderr, "set null array elem ptra returned %d\n", werr);

  ptrb = weed_get_voidptr_array(plant, "ptrs", &werr);
  fprintf(stderr, "get void ** returned (%p %p %p %p) %d\n", ptrb[0], ptrb[1], ptrb[2], ptrb[3], werr);

  weed_leaf_set(plant, "strings", WEED_SEED_STRING, 4, &s);
  fprintf(stderr, "set char ** %d\n", werr);

  char **stng2;
  stng2 = weed_get_string_array(plant, "strings", &werr);
  fprintf(stderr, "get char ** returned (%s %s %s %s) %d\n", stng2[0], stng2[1], stng2[2], stng2[3], werr);

  werr = weed_leaf_set(plant, "arrawn", WEED_SEED_VOIDPTR, 4, ptra);
  fprintf(stderr, "set null array returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_leaf_set_flags(plant, "Test2", 0);
  flags = weed_leaf_get_flags(plant, "string2");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_UNDELETABLE | WEED_FLAG_IMMUTABLE);
  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get type flags returned %d\n", flags);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_leaf_set_flags(plant, "type", WEED_FLAG_IMMUTABLE);
  flags = weed_leaf_get_flags(plant, "type");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "arrawn", WEED_FLAG_UNDELETABLE);
  fprintf(stderr, "set flags returned %d\n", werr);
  flags = weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "indirect", WEED_FLAG_UNDELETABLE);
  werr = weed_leaf_set_flags(plant, "Test2", WEED_FLAG_UNDELETABLE);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);

  werr = weed_leaf_set_flags(plant, "arrawn", 0);
  fprintf(stderr, "set flags returned %d\n", werr);
  flags = weed_leaf_get_flags(plant, "arrawn");
  fprintf(stderr, "get flags returned %d\n", flags);

  werr = weed_leaf_set_flags(plant, "indirect", WEED_FLAG_IMMUTABLE);
  werr = weed_leaf_set_flags(plant, "Test2", 0);

  werr = weed_plant_free(plant);
  fprintf(stderr, "wpf returned %d\n", werr);

  keys = weed_plant_list_leaves(plant, &nleaves);
  n = 0;
  while (keys[n] != NULL) {
    fprintf(stderr, "key %d is %s\n", n, keys[n]);
    free(keys[n]);
    n++;
  }
  free(keys);
  show_timer_info();

#define BPLANT_LEAVES 10000
  g_print("Big plant test: \n");
  g_print("adding %d leaves\n", BPLANT_LEAVES);
  plant = weed_plant_new(WEED_PLANT_EVENT);
  for (int i = 0; i < BPLANT_LEAVES; i++) {
    int num = fastrand() >> 32;
    char *key = lives_strdup_printf("leaf_number_%d", i);
    weed_set_int_value(plant, key, num);
    free(key);
  }
  g_print("done\n");
  g_print("test %d random reads\n", BPLANT_LEAVES * 10);
  n = 0;
  for (int i = 0; i < BPLANT_LEAVES * 10; i++) {
    char *key = lives_strdup_printf("leaf_number_%d", (int)((double)fastrand() / (double)LIVES_MAXUINT64 * BPLANT_LEAVES * 2.));
    weed_get_int_value(plant, key, &werr);
    if (werr == WEED_SUCCESS) n++;
    free(key);
  }
  g_print("done, hit percentage was %.2f\n", (double)n / (double)BPLANT_LEAVES * 10);
  show_timer_info();

  g_print("freeing big plant\n");
  weed_plant_free(plant);
  g_print("done\n");

  return 0;
}

#endif

#define SCALE_FACT 65793. /// (2 ^ 24 - 1) / (2 ^ 8 - 1)

int test_palette_conversions(void) {
  double val;
  int inval, outval;
  for (val = 0.; val < 256.; val += .1) {
    inval = val * SCALE_FACT;
    outval = round_special(inval);
    if (fabs((float)outval - val) > .51)
      g_print("in val was %.6f, stored as %d, returned as %d\n", val, inval, outval);
  }
  return 0;
}


