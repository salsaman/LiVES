/* Frei0r wrapper for Weed
   author: Salsaman (G. Finch) <salsaman@gmail.com>

   Released under the Gnu Public License (GPL) 3 or later
   See www.gnu.org for details

   (c) 2005 - 2012, Salsaman
*/

#include <frei0r.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

///////////////////////////////////////////////////////////////////
#define NEED_ALPHA_SORT

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#include "weed-plugin-utils.c" // optional

static int verbosity = WEED_VERBOSITY_ERROR;

/////////////////////////////////////////////////////////////
typedef f0r_instance_t (*f0r_construct_f)(unsigned int width, unsigned int height);
typedef void (*f0r_destruct_f)(f0r_instance_t instance);
typedef void (*f0r_deinit_f)(void);
typedef int (*f0r_init_f)(void);
typedef void (*f0r_get_plugin_info_f)(f0r_plugin_info_t *info);
typedef void (*f0r_get_param_info_f)(f0r_param_info_t *info, int param_index);
typedef void (*f0r_update_f)(f0r_instance_t instance, double time, const uint32_t *inframe, uint32_t *outframe);
typedef void (*f0r_update2_f)(f0r_instance_t instance, double time, const uint32_t *inframe1, const uint32_t *inframe2,
                              const uint32_t *inframe3, uint32_t *outframe);
typedef void (*f0r_set_param_value_f)(f0r_instance_t *instance, f0r_param_t *param, int param_index);

#if FREI0R_MAJOR_VERSION > 1 || FREI0R_MINOR_VERSION > 1
#define CAN_GET_DEF
typedef void (*f0r_get_param_value_f)(f0r_instance_t *instance, f0r_param_t *param, int param_index);
#endif

#include <limits.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#define HPATH_MAX (PATH_MAX >> 1)
#define HHPATH_MAX (PATH_MAX >> 2)
#define QPATH_MAX (HHPATH_MAX * 3)

static int getenv_piece(char *target, size_t tlen, char *envvar, int num) {
  // get num piece from envvar path and set in target
  char *str1;
  memset(target, 0, 1);

  /* extract first string from string sequence */
  str1 = strtok(envvar, ":");

  /* loop until finishied */
  while (--num >= 0) {
    /* extract string from string sequence */
    str1 = strtok(NULL, ":");

    /* check if there is nothing else to extract */
    if (str1 == NULL) break;
  }

  if (str1 != NULL) snprintf(target, tlen, "%s", str1);
  else return 0;
  return 1;
}


////////////////////////////////////////////////////////////////

static weed_error_t frei0r_init(weed_plant_t *inst) {
  weed_plant_t *out_channel, *filter;
  int height, width, cpalette;
  f0r_instance_t f0r_inst;
  f0r_construct_f f0r_construct;

  filter = weed_instance_get_filter(inst);

  out_channel = weed_get_out_channel(inst, 0);
  width = weed_channel_get_width(out_channel) * 4;
  height = weed_channel_get_height(out_channel);
  cpalette = weed_channel_get_palette(out_channel);

  if (cpalette == WEED_PALETTE_UYVY || cpalette == WEED_PALETTE_YUYV) width >>= 1;
  else width >>= 2;

  f0r_construct = weed_get_voidptr_value(filter, "plugin_f0r_construct", NULL);

  if ((f0r_inst = (*f0r_construct)(width, height)) == NULL) return WEED_ERROR_FILTER_INVALID;
  weed_set_voidptr_value(inst, "plugin_f0r_inst", f0r_inst);

  return WEED_SUCCESS;
}


static weed_error_t frei0r_deinit(weed_plant_t *inst) {
  f0r_instance_t f0r_inst;
  f0r_destruct_f f0r_destruct;
  weed_plant_t *filter;

  filter = weed_instance_get_filter(inst);

  f0r_inst = weed_get_voidptr_value(inst, "plugin_f0r_inst", NULL);
  f0r_destruct = weed_get_voidptr_value(filter, "plugin_f0r_destruct", NULL);
  (*f0r_destruct)(f0r_inst);

  return WEED_SUCCESS;
}


static void weed_params_to_frei0r_params(weed_plant_t *inst, weed_plant_t **in_params, int num_weed_params) {
  double vald, vald2;
  double *cols;
  f0r_instance_t f0rinst = weed_get_voidptr_value(inst, "plugin_f0r_inst", NULL);
  weed_plant_t *filter = weed_instance_get_filter(inst), *ptmpl;
  f0r_set_param_value_f f0r_set_param_value = weed_get_voidptr_value(filter, "plugin_f0r_set_param_value", NULL);
  f0r_param_position_t f0rpos;
  f0r_param_color_t f0rcol;
  int pnum = 0;
  int vali;
  char *string;

  for (int i = 0; i < num_weed_params; i++) {
    int type = weed_param_get_type(in_params[i]);
    switch (type) {
    case WEED_PARAM_SWITCH:
      vali = weed_param_get_value_boolean(in_params[i]);
      vald = (double)vali;
      (*f0r_set_param_value)(f0rinst, (f0r_param_t)&vald, pnum);
      break;
    case WEED_PARAM_FLOAT:
      vald = weed_param_get_value_double(in_params[i]);
      ptmpl = weed_param_get_template(in_params[i]);
      if (!weed_plant_has_leaf(ptmpl, "plugin_f0r_position"))(*f0r_set_param_value)(f0rinst, (f0r_param_t)&vald, pnum);
      else {
        i++;
        vald2 = weed_param_get_value_double(in_params[i]);
        f0rpos.x = vald;
        f0rpos.y = vald2;
        (*f0r_set_param_value)(f0rinst, (f0r_param_t)&f0rpos, pnum);
      }
      break;
    case WEED_PARAM_COLOR:
      cols = weed_param_get_array_double(in_params[i], NULL);
      f0rcol.r = cols[0];
      f0rcol.g = cols[1];
      f0rcol.b = cols[2];
      (*f0r_set_param_value)(f0rinst, (f0r_param_t)&f0rcol, pnum);
      weed_free(cols);
      break;
    case WEED_PARAM_TEXT:
      string = weed_param_get_value_string(in_params[i]);
      (*f0r_set_param_value)(f0rinst, (f0r_param_t)&string, pnum);
      weed_free(string);
      break;
    }
    pnum++;
  }
}


static weed_error_t frei0r_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  f0r_instance_t f0r_inst;
  f0r_update_f f0r_update;
  f0r_update2_f f0r_update2;
  weed_plant_t **in_channels, **in_params;
  weed_plant_t *out_channel, *in_channel, *filter;
  int f0r_plugin_type;

  double time = (double)timestamp / (double)WEED_TICKS_PER_SECOND;

  filter = weed_instance_get_filter(inst);
  f0r_inst = weed_get_voidptr_value(inst, "plugin_f0r_inst", NULL);
  f0r_plugin_type = weed_get_int_value(filter, "plugin_f0r_type", NULL);

  if (weed_plant_has_leaf(inst, WEED_LEAF_IN_PARAMETERS) &&
      (in_params = weed_get_in_params(inst, NULL)) != NULL) {
    weed_params_to_frei0r_params(inst, in_params, weed_leaf_num_elements(inst, WEED_LEAF_IN_PARAMETERS));
  }

  switch (f0r_plugin_type) {
  case F0R_PLUGIN_TYPE_SOURCE:
    f0r_update = weed_get_voidptr_value(filter, "plugin_f0r_update", NULL);
    out_channel = weed_get_out_channel(inst, 0);
    (*f0r_update)(f0r_inst, time, NULL, weed_channel_get_pixel_data(out_channel));
    break;
  case F0R_PLUGIN_TYPE_FILTER:
    f0r_update = weed_get_voidptr_value(filter, "plugin_f0r_update", NULL);
    out_channel = weed_get_out_channel(inst, 0);
    in_channel = weed_get_in_channel(inst, 0);
    (*f0r_update)(f0r_inst, time, weed_channel_get_pixel_data(in_channel),
                  weed_channel_get_pixel_data(out_channel));
    break;
  case F0R_PLUGIN_TYPE_MIXER2:
    f0r_update2 = weed_get_voidptr_value(filter, "plugin_f0r_update2", NULL);
    out_channel = weed_get_out_channel(inst, 0);
    in_channels = weed_get_in_channels(inst, NULL);
    (*f0r_update2)(f0r_inst, time, weed_channel_get_pixel_data(in_channels[0]),
                   weed_channel_get_pixel_data(in_channels[1]), NULL, weed_channel_get_pixel_data(out_channel));
    weed_free(in_channels);
    break;
  case F0R_PLUGIN_TYPE_MIXER3:
    f0r_update2 = weed_get_voidptr_value(filter, "plugin_f0r_update2", NULL);
    out_channel = weed_get_out_channel(inst, 0);
    in_channels = weed_get_in_channels(inst, NULL);
    (*f0r_update2)(f0r_inst, time, weed_channel_get_pixel_data(in_channels[0]),
                   weed_channel_get_pixel_data(in_channels[1]),
                   weed_channel_get_pixel_data(in_channels[2]),
                   weed_channel_get_pixel_data(out_channel));
    weed_free(in_channels);
    break;
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  if (FREI0R_MAJOR_VERSION < 1 || FREI0R_MINOR_VERSION < 1) return NULL;
  dlink_list_t *list = NULL;
  int *pal;

  char **rfx_strings = NULL;

  weed_plant_t **out_chantmpls, **in_chantmpls = NULL;
  weed_plant_t **in_params = NULL, *gui, *pgui;

  weed_plant_t *filter_class;
  f0r_plugin_info_t f0rinfo;
  f0r_param_info_t pinfo;

  int finished = 0;
  int pnum, wnum, num_weed_params;

  char vdir1[HPATH_MAX] = "/usr/lib/frei0r-1/";
  char vdir2[HPATH_MAX] = "/usr/local/lib/frei0r-1/";
  char vdir3[HPATH_MAX];

  char dir1[QPATH_MAX], dir2[QPATH_MAX], dir3[QPATH_MAX];
  char plug1[PATH_MAX], plug2[PATH_MAX], plug3[PATH_MAX];

  struct dirent *vdirent = NULL, *dirent;

  char homedir[HPATH_MAX - 10];

  char vendor_name[HHPATH_MAX], plugin_name[HHPATH_MAX], weed_name[PATH_MAX];

  DIR *curvdir = NULL, *curdir = NULL;

  int vdirval = 0;

  void *handle = NULL;

  int pversion;

  char *label;

  f0r_deinit_f f0r_deinit;
  f0r_init_f f0r_init;
  f0r_get_plugin_info_f f0r_get_plugin_info;
  f0r_get_param_info_f f0r_get_param_info;
  f0r_construct_f f0r_construct;
  f0r_destruct_f f0r_destruct;
  f0r_update_f f0r_update = NULL;
  f0r_update2_f f0r_update2 = NULL;
  f0r_set_param_value_f f0r_set_param_value = NULL;

  double vald;
  char *valch;

  f0r_param_color_t valcol;
  f0r_param_position_t valpos;

  //int api_used = weed_get_api_version(plugin_info);

  int is_unstable;

  int num_filters = 0;

#ifdef CAN_GET_DEF
  f0r_instance_t f0r_inst;
  f0r_get_param_value_f f0r_get_param_value = NULL;
#endif

  char *fpp = getenv("FREI0R_PATH");

  verbosity = weed_get_host_verbosity(weed_get_host_info(plugin_info));

  weed_set_string_value(plugin_info, WEED_LEAF_PACKAGE_NAME, "Frei0r");

  if (fpp != NULL) {
    vdirval = 10;
  } else {
    snprintf(homedir, HPATH_MAX - 10, "%s", getenv("HOME"));
    snprintf(vdir3, HPATH_MAX, "%s/frei0r-1/", homedir);
  }

  while (vdirval < 6 || vdirval > 9) {
    // step through each of our frei0r dirs
    if (curvdir != NULL) closedir(curvdir);
    curvdir = NULL;

    if (vdirval == 0) {
      curvdir = opendir(vdir3);
      if (curvdir == NULL) vdirval = 2;
      else vdirval = 1;
    }

    if (vdirval == 2) {
      curvdir = opendir(vdir2);
      if (curvdir == NULL) vdirval = 4;
      else {
        vdirval = 3;
      }
    }

    if (vdirval == 4) {
      curvdir = opendir(vdir1);
      if (curvdir == NULL) {
        vdirval = 6;
        break;
      }
      vdirval = 5;
    }

    if (vdirval > 9) {
      char *fpp_copy = strdup(fpp);
      if (!getenv_piece(vdir1, HPATH_MAX, fpp_copy, vdirval - 10)) {
        free(fpp_copy);
        vdirval = 6;
        break;
      }
      free(fpp_copy);

      if (!strlen(vdir1)) {
        vdirval++;
        continue;
      }

      curvdir = opendir(vdir1);
      if (curvdir == NULL) {
        vdirval++;
        continue;
      }
    }

    weed_memset(vendor_name, 0, 1);

    do {
      if (curdir != NULL) closedir(curdir);
      curdir = NULL;

      snprintf(dir1, QPATH_MAX, "%s/%s", vdir1, vendor_name);

      if (vdirval < 10) {
        snprintf(dir2, QPATH_MAX, "%s/%s", vdir2, vendor_name);
        snprintf(dir3, QPATH_MAX, "%s/%s", vdir3, vendor_name);
      }

      vdirent = readdir(curvdir);

      if (vdirent != NULL) {
        if (!strncmp(vdirent->d_name, "..", strlen(vdirent->d_name))) continue;
        snprintf(vendor_name, HHPATH_MAX, "%s", vdirent->d_name);
      }

      if (vdirval == 1) {
        curdir = opendir(dir3);
        if (curdir == NULL) {
          if (vdirent == NULL) break;
          continue;
        }
      } else if (vdirval == 3) {
        curdir = opendir(dir2);
        if (curdir == NULL) {
          if (vdirent == NULL) break;
          continue;
        }
      } else if (vdirval == 5 || vdirval > 9) {
        curdir = opendir(dir1);
        if (curdir == NULL) {
          if (vdirent == NULL) break;
          continue;
        }
      }

      finished = 0;

      while (!finished) {
        // step through our plugins
        dirent = readdir(curdir);

        if (dirent == NULL) {
          finished = 1;
          continue;
        }

        if (!strncmp(dirent->d_name, "..", strlen(dirent->d_name))) continue;

        snprintf(plugin_name, HHPATH_MAX, "%s", dirent->d_name);

        snprintf(plug1, PATH_MAX, "%s/%s", dir1, plugin_name);

        if (vdirval < 10) {

          snprintf(plug2, PATH_MAX, "%s/%s", dir2, plugin_name);
          snprintf(plug3, PATH_MAX, "%s/%s", dir3, plugin_name);

          handle = dlopen(plug3, RTLD_NOW);
          if ((handle != NULL && (vdirval > 1)) || (handle == NULL && vdirval == 1)) {
            if (handle != NULL) dlclose(handle);
            handle = NULL;
            continue;
          }

          if (vdirval > 1) {
            handle = dlopen(plug2, RTLD_NOW);
            if ((handle != NULL && (vdirval > 3)) || (handle == NULL && vdirval == 3)) {
              if (handle != NULL) dlclose(handle);
              handle = NULL;
              continue;
            }
          }
        }

        if (vdirval == 5 || vdirval > 9) {
          handle = dlopen(plug1, RTLD_NOW);
          if (handle == NULL) continue;
        }

        if ((f0r_deinit = dlsym(handle, "f0r_deinit")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if ((f0r_init = dlsym(handle, "f0r_init")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if ((f0r_get_plugin_info = dlsym(handle, "f0r_get_plugin_info")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if ((f0r_get_param_info = dlsym(handle, "f0r_get_param_info")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if ((f0r_construct = dlsym(handle, "f0r_construct")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if ((f0r_destruct = dlsym(handle, "f0r_destruct")) == NULL) {
          dlclose(handle);
          handle = NULL;
          continue;
        }

        is_unstable = 0;

        (*f0r_init)();
        (*f0r_get_plugin_info)(&f0rinfo);

        if (f0rinfo.frei0r_version != FREI0R_MAJOR_VERSION) {
          (*f0r_deinit)();
          dlclose(handle);
          handle = NULL;
          continue;
        }

        switch (f0rinfo.plugin_type) {
        case F0R_PLUGIN_TYPE_SOURCE:
        case F0R_PLUGIN_TYPE_FILTER:
          if ((f0r_update = dlsym(handle, "f0r_update")) == NULL) {
            (*f0r_deinit)();
            dlclose(handle);
            handle = NULL;
            continue;
          }
          break;
        case F0R_PLUGIN_TYPE_MIXER2:
        case F0R_PLUGIN_TYPE_MIXER3:
          if ((f0r_update2 = dlsym(handle, "f0r_update2")) == NULL) {
            (*f0r_deinit)();
            dlclose(handle);
            handle = NULL;
            continue;
          }
          break;
        default:
          (*f0r_deinit)();
          dlclose(handle);
          handle = NULL;
          continue;
        }

        if (f0rinfo.color_model == F0R_COLOR_MODEL_BGRA8888) {
          pal = weed_malloc(2 * sizeof(int));
          pal[0] = WEED_PALETTE_BGRA8888;
          pal[1] = WEED_PALETTE_END;
        } else if (f0rinfo.color_model == F0R_COLOR_MODEL_RGBA8888) {
          pal = weed_malloc(2 * sizeof(int));
          pal[0] = WEED_PALETTE_RGBA8888;
          pal[1] = WEED_PALETTE_END;
        } else if (f0rinfo.color_model == F0R_COLOR_MODEL_PACKED32) {
          pal = weed_malloc(7 * sizeof(int));
          pal[0] = WEED_PALETTE_RGBA8888;
          pal[1] = WEED_PALETTE_BGRA8888;
          pal[2] = WEED_PALETTE_ARGB8888;
          pal[3] = WEED_PALETTE_UYVY8888;
          pal[4] = WEED_PALETTE_YUYV8888;
          pal[5] = WEED_PALETTE_YUVA8888;
          pal[6] = WEED_PALETTE_END;
        } else {
          f0r_deinit();
          dlclose(handle);
          handle = NULL;
          continue;
        }

        out_chantmpls = weed_malloc(2 * sizeof(weed_plant_t *));
        out_chantmpls[0] = weed_channel_template_init("out channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
        out_chantmpls[1] = NULL;

        switch (f0rinfo.plugin_type) {
        case F0R_PLUGIN_TYPE_SOURCE:
          in_chantmpls = NULL;
          break;
        case F0R_PLUGIN_TYPE_FILTER:
          in_chantmpls = weed_malloc(2 * sizeof(weed_plant_t *));
          in_chantmpls[0] = weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[1] = NULL;
          break;
        case F0R_PLUGIN_TYPE_MIXER2:
          in_chantmpls = weed_malloc(3 * sizeof(weed_plant_t *));
          in_chantmpls[0] = weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[1] = weed_channel_template_init("in channel 1", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[2] = NULL;
          break;
        case F0R_PLUGIN_TYPE_MIXER3:
          in_chantmpls = weed_malloc(4 * sizeof(weed_plant_t *));
          in_chantmpls[0] = weed_channel_template_init("in channel 0", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[1] = weed_channel_template_init("in channel 1", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[2] = weed_channel_template_init("in channel 2", WEED_CHANNEL_REINIT_ON_SIZE_CHANGE);
          in_chantmpls[3] = NULL;
          break;
        default:
          (*f0r_deinit)();
          dlclose(handle);
          handle = NULL;
          weed_free(pal);
          weed_free(out_chantmpls);
          if (in_chantmpls != NULL) weed_free(in_chantmpls);
          continue;
        }

        num_weed_params = 0;

#ifdef CAN_GET_DEF
        // try to get defaults
        if ((f0r_inst = (*f0r_construct)(640, 480)) == NULL) {
          (*f0r_deinit)();
          dlclose(handle);
          handle = NULL;
          weed_free(out_chantmpls);
          if (in_chantmpls != NULL) weed_free(in_chantmpls);
          continue;
        }
#endif

        if (f0rinfo.num_params > 0) {
          if ((f0r_set_param_value = dlsym(handle, "f0r_set_param_value")) == NULL) {
#ifdef CAN_GET_DEF
            f0r_destruct(f0r_inst);
#endif
            (*f0r_deinit)();
            dlclose(handle);
            handle = NULL;
            weed_free(out_chantmpls);
            if (in_chantmpls != NULL) weed_free(in_chantmpls);
            continue;
          }

#ifdef CAN_GET_DEF
          if ((f0r_get_param_value = dlsym(handle, "f0r_get_param_value")) == NULL) {
            f0r_destruct(f0r_inst);
            (*f0r_deinit)();
            dlclose(handle);
            handle = NULL;
            weed_free(out_chantmpls);
            if (in_chantmpls != NULL) weed_free(in_chantmpls);
            continue;
          }
#endif

          for (pnum = 0; pnum < f0rinfo.num_params; pnum++) {
            num_weed_params++;
            (*f0r_get_param_info)(&pinfo, pnum);
            if (pinfo.type == F0R_PARAM_POSITION) num_weed_params++;
          }

          if (num_weed_params > f0rinfo.num_params) {
            rfx_strings = weed_malloc((f0rinfo.num_params) * sizeof(char *));
            for (pnum = 0; pnum < f0rinfo.num_params; pnum++) {
              rfx_strings[pnum] = (char *)weed_malloc(256);
            }
          }
          in_params = weed_malloc((num_weed_params + 1) * sizeof(weed_plant_t *));

          wnum = 0;
          for (pnum = 0; pnum < f0rinfo.num_params; pnum++) {
            (*f0r_get_param_info)(&pinfo, pnum);
            label = malloc(strlen((char *)pinfo.name) + 2);
            sprintf(label, "_%s", (char *)pinfo.name);
            switch (pinfo.type) {
            case F0R_PARAM_BOOL:
              vald = 0.;
#ifdef CAN_GET_DEF
              f0r_get_param_value(f0r_inst, (void **)&vald, pnum);
              if (vald != 0. && vald != 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\n(%f) "
                          "for boolean parameter %s.\nThis plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, vald, pinfo.name);
                is_unstable = 1;
              }
              if (vald <= 0.5) vald = 0.;
              else vald = 1.;
#endif
              in_params[wnum] = weed_switch_init((char *)pinfo.name, label, vald == 0. ? WEED_FALSE : WEED_TRUE);
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              if (num_weed_params > f0rinfo.num_params) sprintf(rfx_strings[pnum], "layout|p%d|", wnum);
              break;
            case F0R_PARAM_DOUBLE:
              vald = 0.;
#ifdef CAN_GET_DEF
              f0r_get_param_value(f0r_inst, (void **)&vald, pnum);

              if (vald < 0. || vald > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\n(%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, vald, pinfo.name);
                is_unstable = 1;
                if (vald < 0.) vald = 0.;
                if (vald > 1.) vald = 1.;
              }

#endif
              in_params[wnum] = weed_float_init((char *)pinfo.name, label, vald, 0., 1.);
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              if (num_weed_params > f0rinfo.num_params) sprintf(rfx_strings[pnum], "layout|p%d|", wnum);
              pgui = weed_paramtmpl_get_gui(in_params[wnum]);
              weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, .01);
              weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);
              break;
            case F0R_PARAM_COLOR:
              valcol.r = valcol.g = valcol.b = 0.;
#ifdef CAN_GET_DEF
              f0r_get_param_value(f0r_inst, (void **)&valcol, pnum);

              if (valcol.r < 0. || valcol.r > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\nred (%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, valcol.r, pinfo.name);
                is_unstable = 1;
                if (valcol.r < 0.) valcol.r = 0.;
                if (valcol.r > 1.) valcol.r = 1.;
              }

              if (valcol.g < 0. || valcol.g > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\ngreen (%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, valcol.g, pinfo.name);
                is_unstable = 1;
                if (valcol.g < 0.) valcol.g = 0.;
                if (valcol.g > 1.) valcol.g = 1.;
              }

              if (valcol.b < 0. || valcol.b > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\nblue (%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, valcol.b, pinfo.name);
                is_unstable = 1;
                if (valcol.b < 0.) valcol.b = 0.;
                if (valcol.b > 1.) valcol.b = 1.;
              }

#endif
              in_params[wnum] = weed_colRGBd_init((char *)pinfo.name, label, valcol.r, valcol.g, valcol.b);
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              if (num_weed_params > f0rinfo.num_params) sprintf(rfx_strings[pnum], "layout|p%d|", wnum);
              break;
            case F0R_PARAM_POSITION:
              valpos.x = valpos.y = 0.;
#ifdef CAN_GET_DEF
              f0r_get_param_value(f0r_inst, (void **)&valpos, pnum);
              if (valpos.x < 0. || valpos.x > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\nx-pos (%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, valpos.x, pinfo.name);
                is_unstable = 1;
                if (valpos.x < 0.) valpos.x = 0.;
                if (valpos.x > 1.) valpos.x = 1.;
              }

              if (valpos.y < 0. || valpos.y > 1.) {
                if (verbosity >= WEED_VERBOSITY_WARN)
                  fprintf(stderr, "Warning, frei0r plugin '%s' by %s sets bad default value\ny-pos (%f) for parameter %s.\n"
                          "This plugin may be unstable !\n",
                          f0rinfo.name, f0rinfo.author, valpos.y, pinfo.name);
                is_unstable = 1;
                if (valpos.y < 0.) valpos.y = 0.;
                if (valpos.y > 1.) valpos.y = 1.;
              }
#endif
              in_params[wnum] = weed_float_init((char *)pinfo.name, label, valpos.x, 0., 1.);
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              weed_set_boolean_value(in_params[wnum], "plugin_is_position", WEED_TRUE);
              pgui = weed_paramtmpl_get_gui(in_params[wnum]);
              weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, .01);
              weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);
              wnum++;
              in_params[wnum] = weed_float_init((char *)pinfo.name, "", valpos.y, 0., 1.);
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              sprintf(rfx_strings[pnum], "layout|p%d|\"X\"|fill|p%d|\"Y\"|fill|", wnum - 1, wnum);
              pgui = weed_paramtmpl_get_gui(in_params[wnum]);
              weed_set_double_value(pgui, WEED_LEAF_STEP_SIZE, .01);
              weed_set_int_value(pgui, WEED_LEAF_DECIMALS, 2);
              break;
            case F0R_PARAM_STRING:
#ifdef CAN_GET_DEF
              f0r_get_param_value(f0r_inst, (void **)&valch, pnum);
#else
              valch = strdup("Frei0r");
#endif
              in_params[wnum] = weed_text_init((char *)pinfo.name, label, valch);
#ifndef CAN_GET_DEF
              free(valch);
#endif
              weed_set_string_value(in_params[wnum], WEED_LEAF_DESCRIPTION, (char *)pinfo.explanation);
              if (num_weed_params > f0rinfo.num_params) sprintf(rfx_strings[pnum], "layout|p%d|", wnum);
              break;
            default:
#ifdef CAN_GET_DEF
              f0r_destruct(f0r_inst);
#endif
              (*f0r_deinit)();
              dlclose(handle);
              handle = NULL;
              weed_free(out_chantmpls);
              if (in_chantmpls != NULL) weed_free(in_chantmpls);
              if (in_params != NULL) weed_free(in_params);
              if (rfx_strings != NULL) {
                int j;
                for (j = 0; j < pnum; j++) weed_free(rfx_strings[j]);
                weed_free(rfx_strings);
              }
              continue;
            }
            free(label);
            wnum++;
          }
          in_params[pnum] = NULL;
        }

#ifdef CAN_GET_DEF
        f0r_destruct(f0r_inst);
#endif

        snprintf(weed_name, PATH_MAX, "%s", f0rinfo.name);
        pversion = f0rinfo.major_version * 1000 + f0rinfo.minor_version;

        filter_class = weed_filter_class_init(weed_name, "Frei0r developers", pversion, 0, pal, frei0r_init, frei0r_process,
                                              frei0r_deinit, in_chantmpls, out_chantmpls, in_params, NULL);
        weed_free(pal);

        weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, (char *)f0rinfo.author);

        if (is_unstable) weed_filter_set_flags(filter_class, weed_filter_get_flags(filter_class) | WEED_FILTER_HINT_MAYBE_UNSTABLE);

        weed_set_int_value(filter_class, WEED_LEAF_HSTEP, 8);
        weed_set_int_value(filter_class, WEED_LEAF_VSTEP, 8);
        weed_set_int_value(filter_class, WEED_LEAF_MAXWIDTH, 2048);
        weed_set_int_value(filter_class, WEED_LEAF_MAXHEIGHT, 2048);
        weed_set_int_value(filter_class, WEED_LEAF_ALIGNMENT_HINT, 16);

        if (num_weed_params > f0rinfo.num_params) {
          gui = weed_filter_get_gui(filter_class);
          weed_set_string_value(gui, WEED_LEAF_LAYOUT_SCHEME, "RFX");
          weed_set_string_value(gui, "layout_rfx_delim", "|");
          weed_set_string_array(gui, "layout_rfx_strings", f0rinfo.num_params, rfx_strings);
          for (wnum = 0; wnum < f0rinfo.num_params; wnum++) weed_free(rfx_strings[wnum]);
          weed_free(rfx_strings);
          rfx_strings = NULL;
        }

        if (f0rinfo.explanation != NULL) weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, (char *)f0rinfo.explanation);
        num_filters++;
        weed_free(out_chantmpls);
        if (in_chantmpls != NULL) weed_free(in_chantmpls);
        if (in_params != NULL) weed_free(in_params);
        in_params = NULL;

        weed_set_voidptr_value(filter_class, "plugin_handle", handle);

        weed_set_voidptr_value(filter_class, "plugin_f0r_construct", f0r_construct);
        weed_set_voidptr_value(filter_class, "plugin_f0r_destruct", f0r_destruct);
        if (f0rinfo.num_params > 0) weed_set_voidptr_value(filter_class, "plugin_f0r_set_param_value", f0r_set_param_value);
        weed_set_int_value(filter_class, "plugin_f0r_type", f0rinfo.plugin_type);

        switch (f0rinfo.plugin_type) {
        case F0R_PLUGIN_TYPE_SOURCE:
          weed_set_voidptr_value(filter_class, "plugin_f0r_update", f0r_update);
          break;
        case F0R_PLUGIN_TYPE_FILTER:
          weed_set_voidptr_value(filter_class, "plugin_f0r_update", f0r_update);
          weed_set_int_value(filter_class, WEED_LEAF_FLAGS, WEED_FILTER_PREF_LINEAR_GAMMA);
          break;
        default:
          weed_set_voidptr_value(filter_class, "plugin_f0r_update2", f0r_update2);
          weed_set_int_value(filter_class, WEED_LEAF_FLAGS, WEED_FILTER_PREF_LINEAR_GAMMA);
          break;
        }

        list = add_to_list_sorted(list, filter_class, f0rinfo.name);

        // end plugin
      }
      // end vendor dir
    } while (vdirent != NULL);
    if (curvdir != NULL) closedir(curvdir);
    curvdir = NULL;
    vdirval++;
    if (curdir != NULL) closedir(curdir);
    curdir = NULL;
  }
  // end frei0r dirs

  if (curvdir != NULL) closedir(curvdir);
  curvdir = NULL;
  if (curdir != NULL) closedir(curdir);
  curdir = NULL;

#ifndef DEBUG
  /* dup2(new_stdout, 1); */
  /* dup2(new_stderr, 2); */
#endif

  if (num_filters == 0) {
    if (verbosity >= WEED_VERBOSITY_CRITICAL)
      fprintf(stderr,
              "No frei0r plugins found; if you have them installed please set the FREI0R_PATH environment variable to point to them.\n");
    return NULL;
  }

  add_filters_from_list(plugin_info, list);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;



