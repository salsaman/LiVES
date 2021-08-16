// system.c
// weed plugin
// (c) G. Finch (salsaman) 2021
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// generate a random double when input changes state

//#define DEBUG

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

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

#include "weed-plugin-utils.c"

/////////////////////////////////////////////////////////////

#include <stdio.h>
#include <errno.h>

#define BUFFLEN 65536

static int verbosity = WEED_VERBOSITY_WARN;

enum {
  P_TRIG,
  P_CMD,
  P_TRIG_FT,
  P_TRIG_TF,
  P_END
};

static weed_error_t system_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t **out_params = weed_get_out_params(inst, NULL);
  int oldval = weed_get_boolean_value(inst, "plugin_oldval", NULL);
  int newval = weed_param_get_value_boolean(in_params[P_TRIG]);
  if (oldval != newval) {
    int trigt = weed_param_get_value_boolean(in_params[P_TRIG_FT]);
    int trigf = weed_param_get_value_boolean(in_params[P_TRIG_TF]);
    if ((oldval == WEED_FALSE && trigt == WEED_TRUE) || (oldval == WEED_TRUE && trigf == WEED_TRUE)) {
      FILE *fp;
      char buff[BUFFLEN];
      char *com = weed_param_get_value_string(in_params[P_CMD]);
      size_t slen = 0;
      int err = 0, res = -1;
      fflush(NULL);
      fp = popen(com, "r");
      if (!fp) err = errno;
      else {
        char *strg = NULL;
        while (1) {
          strg = fgets(buff + slen, BUFFLEN - slen, fp);
          err = ferror(fp);
          if (strg) slen = strlen(buff);
          if (err != 0 || !strg || !*strg) break;
          if (slen >= BUFFLEN - 1) break;
        }
        res = pclose(fp);
      }
      if (err) {
        if (verbosity >= WEED_VERBOSITY_WARN) {
          fprintf(stderr, "Weed system plugin failed after %ld bytes with code %d: %s",
                  slen, err, com);
        }
        weed_set_string_value(out_params[0], WEED_LEAF_VALUE, NULL);
      } else weed_set_string_value(out_params[0], WEED_LEAF_VALUE, buff);

      weed_set_int_value(out_params[1], WEED_LEAF_VALUE, res);
      weed_set_int_value(out_params[2], WEED_LEAF_VALUE, err);
    }
  }

  weed_free(in_params);
  weed_free(out_params);
  weed_set_boolean_value(inst, "plugin_oldval", newval);
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t *filter_class, *gui;
  weed_plant_t *in_params[P_END + 1];
  weed_plant_t *out_params[4];
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  char desc[512];

  verbosity = weed_get_host_verbosity(host_info);

  in_params[P_TRIG] = weed_switch_init("trugger", "_Trigger", WEED_FALSE);
  gui = weed_paramtmpl_get_gui(in_params[P_TRIG]);
  weed_set_boolean_value(gui, WEED_LEAF_HIDDEN, WEED_TRUE);

  in_params[P_CMD] = weed_text_init("command", "_Command to run", "");
  in_params[P_TRIG_FT] = weed_switch_init("trugger_ft", "Trigger on _FALSE -> TRUE", WEED_TRUE);
  in_params[P_TRIG_TF] = weed_switch_init("trugger_tf", "Trigger on _TRUE -> FALSE", WEED_FALSE);
  in_params[P_END] = NULL;

  out_params[0] = weed_out_param_text_init("Command output", "");
  out_params[1] = weed_out_param_integer_init("Return value", -1, -1, 9999);
  out_params[2] = weed_out_param_integer_init("Error code", 0, -256, 0);
  out_params[3] = NULL;

  filter_class = weed_filter_class_init("system", "salsaman", 1, 0, NULL,
                                        NULL, system_process, NULL, NULL, NULL, in_params, out_params);

  snprintf(desc, 512, "This filter can run any commandline command, and return the output in a\n"
           "text parameter. The command will be run whenevr the trigger input changes state\n"
           "- the trigger direction(s) can be selected via input paramters.\n"
           "The filter also returns the exit status and any error value from running the command.");
  weed_set_string_value(filter_class, WEED_LEAF_DESCRIPTION, desc);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

