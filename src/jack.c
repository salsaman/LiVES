// jack.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2022
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// NB: float jack_get_xrun_delayed_usecs (jack_client_t *); jack_set_xrun_callback;

#include "main.h"
#include <jack/jslist.h>
#include <jack/control.h>
#include <jack/metadata.h>
#include <jack/statistics.h>
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef ENABLE_JACK
#include "callbacks.h"
#include "effects.h"
#include "effects-weed.h"
#include "paramwindow.h"
#include "startup.h"

#define MAX_CONX_TRIES 10

boolean jack_warn(boolean is_trans, boolean is_con) {
  // this function gets called via a signla handler handler:- if the thread trying to
  // open or connect to a jack server hangs for too loknf, then the underlying thread is signalled and should and we
  // should end up here, either via s ignal handler, or via the thread's own "cleanup" function.
  // Here. We ought not to sinply exit the program - the user should have an opportunity to troubleshoot and
  // correct whatever is going wrong. Without this, it could happen that the program becomes unusable as each time
  // we reload the same situation repeats. One solution is to diiable the player in preferences, but still the user has to
  // be able to intervene - it may be some temporary condition - server not started for example which doesnt warrant
  // disableing the player plugin and forcing the user to set it up again. Hence w do as much as is feasible here, given
  // that we are in a signal handler after receiving a non-ignorable, fatal signal. Eventually we have to exit
  // ideally we would allow for going through several cycles of troublehsoot -> alter settings -> retest andonce succesfull,
  // allow the user to continue as normal.
  // At a minimum, we try to be polite and allow the user to restart the program and try again.
  lives_proc_thread_t fork_lpt;
  char *com = NULL;
  boolean ret = TRUE;

  mainw->fatal = TRUE;

  if (mainw && mainw->splash_window) lives_widget_hide(mainw->splash_window);
  if (!is_con) {
    ret = do_jack_no_startup_warn(is_trans);
    //if (!prefs->startup_phase) do_jack_restart_warn(-1, NULL);
  } else {
    ret = do_jack_no_connect_warn(is_trans);
    //if (!prefs->startup_phase) do_jack_restart_warn(16, NULL);
    // as a last ditch attempt, try to launch the server
    if (is_trans) {
      if (prefs->jack_opts & JACK_OPTS_START_TSERVER) {
        if (*future_prefs->jack_tserver_cfg) com = lives_strdup_printf("source '%s'", future_prefs->jack_tserver_cfg);
      }
    } else {
      if (prefs->jack_opts & JACK_OPTS_START_ASERVER) {
        if (*future_prefs->jack_aserver_cfg) com = lives_strdup_printf("source '%s'", future_prefs->jack_aserver_cfg);
      }
    }
    if (com) {
      fork_lpt = lives_hook_append(mainw->global_hook_stacks, RESTART_HOOK, HOOK_OPT_ONESHOT, lives_fork_cb, com);
    }
  }
  // TODO - if we have backup config, restore from it
  if (!ret) {
    maybe_abort(TRUE, mainw->restart_params);
  }
  if (com) {
    lives_hook_remove(fork_lpt);
  }
  return ret;
}


static int start_ready_callback(jack_transport_state_t state, jack_position_t *pos, void *jackd);
//static int xrun_callback(void *jackd);
static void timebase_callback(jack_transport_state_t state, jack_nframes_t nframes, jack_position_t *pos, int new_pos,
                              void *jackd);

#define afile mainw->files[jackd->playing_file]

static lives_audio_buf_t *cache_buffer = NULL;

static unsigned char *zero_buff = NULL;
static size_t zero_buff_count = 0;

static boolean seek_err;

static boolean twins = FALSE;

static boolean ts_started = FALSE;
static boolean as_started = FALSE;

static boolean ts_scripted = FALSE;
static boolean as_scripted = FALSE;

static char *ts_running = NULL;
static char *as_running = NULL;

static boolean is_inited = FALSE;

static lives_pid_t aserver_pid = 0, tserver_pid = 0;

static char last_errmsg[JACK_PARAM_STRING_MAX];

static jackctl_server_t *jackserver = NULL;

static const char **in_ports = NULL;
static const char **out_ports = NULL;

static off_t fwd_seek_pos = 0;

static text_window *tstwin = NULL;
static LiVESTextBuffer *textbuf;

static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int fileno,
                               int nframes, double out_scale, boolean rev_endian, boolean out_unsigned);

/// port connections ////

#define JACK_PULSE_OUT_CLIENT "pulse_out"
// etc,,,

LIVES_GLOBAL_INLINE const char *jack_get_best_client(lives_jack_port_type type, LiVESList *clients) {
  switch (type) {
  case JACK_PORT_TYPE_DEF_IN:
    if (lives_list_locate_string(clients, JACK_PULSE_OUT_CLIENT))
      return JACK_PULSE_OUT_CLIENT;
    break;
  default:
    break;
  }
  return JACK_SYSTEM_CLIENT;
}

const char **jack_get_inports(void) {return in_ports;}
const char **jack_get_outports(void) {return out_ports;}

LiVESList *jack_get_inport_clients(void) {
  LiVESList *list = NULL;
  for (int i = 0; in_ports[i]; i++) {
    char **pieces = lives_strsplit(in_ports[i], ":", 2);
    list = lives_list_append_unique_str(list, pieces[0]);
    lives_strfreev(pieces);
  }
  return list;
}

LiVESList *jack_get_outport_clients(void) {
  LiVESList *list = NULL;
  for (int i = 0; out_ports[i]; i++) {
    char **pieces = lives_strsplit(out_ports[i], ":", 2);
    list = lives_list_append_unique_str(list, pieces[0]);
    lives_strfreev(pieces);
  }
  return list;
}


void jack_conx_exclude(jack_driver_t *jackd_in, jack_driver_t *jackd_out, boolean disc) {
  static const char *xins[JACK_MAX_PORTS], *xouts[JACK_MAX_PORTS];
  const char **ins, **outs;
  jack_port_t **inports = jackd_in->input_port;
  jack_port_t **outports = jackd_out->output_port;
  int i = 0, j, nch = JACK_MAX_PORTS;
  if (disc) {
    for (i = 0; i < nch; i++) {
      // get whatever in[i] is connected to, and whatever out[i], and disconn.
      xins[i] = xouts[i] = NULL;
      if (prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON) continue;
      if (!inports[i]) break;
      ins = jack_port_get_connections(inports[i]);
      if (ins) {
        for (j = 0; j < nch; j ++) {
          if (!outports[j]) break;
          outs = jack_port_get_connections(outports[j]);
          if (outs) {
            // TODO - timeout
            if (!jack_disconnect(jackd_in->client, ins[0], outs[0])) {
              xins[i] = ins[0];
              xouts[i] = outs[0];
            }
            lives_free(outs);
	    // *INDENT-OFF*
	  }}}
      lives_free(ins);
    }}
  // *INDENT-ON*
  else {
    for (i = 0; i < nch; i++) {
      if (xins[i] && xouts[i]) {
        jack_connect(jackd_in->client, xins[i], xouts[i]);
        xins[i] = xouts[i] = NULL;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


#define _SHOW_ERRORS
static void jack_error_func(const char *desc) {
#ifdef _SHOW_ERRORS
  if (prefs->show_dev_opts) {
    lives_printerr("Jack audio error %s\n", desc);
  }
#endif
  lives_snprintf(last_errmsg, JACK_PARAM_STRING_MAX, "%s", desc);
}


static int jack_get_srate(jack_nframes_t nframes, void *arg) {
  //lives_printerr("the sample rate is now %ld/sec\n", (int64_t)nframes);
  // TODO: reset timebase
  jack_driver_t *jackd = (jack_driver_t *)arg;

  if (jackd->client_type != JACK_CLIENT_TYPE_AUDIO_WRITER)
    jackd->sample_in_rate = jack_get_sample_rate(jackd->client);

  if (jackd->client_type != JACK_CLIENT_TYPE_AUDIO_READER)
    jackd->sample_out_rate = jack_get_sample_rate(jackd->client);

  return 0;
}


void jack_shutdown(void *arg) {
  jack_driver_t *jackd = (jack_driver_t *)arg;

  jackd->client = NULL; /* reset client */
  jackd->jackd_died = TRUE;
  jackd->msgq = NULL;

  lives_printerr("jack shutdown, setting client to 0 and jackd_died to true\n");
  lives_printerr("trying to reconnect right now\n");

  /////////////////////

  if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_WRITER) {
    jack_audio_init();
    mainw->jackd = jack_get_driver(0, TRUE);
    if (mainw->jackd->playing_file != -1 && afile)
      jack_audio_seek_bytes(mainw->jackd, mainw->jackd->seek_pos, afile); // at least re-seek to the right place
  }
  if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_READER) {
    jack_audio_read_init();
    mainw->jackd_read = jack_get_driver(0, TRUE);
  }
}


// round int a up to next multiple of int b, unless a is already a multiple of b
LIVES_LOCAL_INLINE int64_t align_ceilng(int64_t val, int mod) {
  return (int64_t)((double)(val + mod - 1.) / (double)mod) * (int64_t)mod;
}


static boolean check_zero_buff(size_t check_size) {
  if (check_size > zero_buff_count) {
    zero_buff = (unsigned char *)lives_realloc(zero_buff, check_size);
    if (zero_buff) {
      lives_memset(zero_buff + zero_buff_count, 0, check_size - zero_buff_count);
      zero_buff_count = check_size;
      return TRUE;
    }
    zero_buff_count = 0;
    return FALSE;
  }
  return TRUE;
}


lives_rfx_t *jack_params_to_rfx(const JSList * dparams, void *source) {
  lives_rfx_t *rfx = rfx_init(RFX_STATUS_INTERFACE, LIVES_RFX_SOURCE_EXTERNAL, source);
  lives_param_t *rpar;
  JSList *zdparams = (JSList *)dparams;
  union jackctl_parameter_value val, def;
  int pcount = 0;

  for (; zdparams; zdparams = zdparams->next) pcount++;
  rpar = rfx_init_params(rfx, pcount);
  pcount = 0;
  for (zdparams = (JSList *)dparams; zdparams; zdparams = zdparams->next) {
    lives_param_t *param = &rpar[pcount];
    jackctl_parameter_t *jparam = (jackctl_parameter_t *)zdparams->data;
    jackctl_param_type_t jtype = jackctl_parameter_get_type(jparam);
    param->name = lives_strdup(jackctl_parameter_get_name(jparam));
    param->label = lives_strdup_printf("%s [--%s]", jackctl_parameter_get_short_description(jparam),
                                       param->name);
    param->desc = lives_strdup(jackctl_parameter_get_long_description(jparam));
    def = jackctl_parameter_get_default_value(jparam);
    val = jackctl_parameter_get_value(jparam);
    if (!strcmp(param->name, "name")) param->hidden = HIDDEN_GUI_PERM;
    switch (jtype) {
    case JackParamUInt:
      param->min = 0.;
      param->dp = 1;
    case JackParamInt:
      param->type = LIVES_PARAM_NUM;
      if (jackctl_parameter_has_range_constraint(jparam)) {
        union jackctl_parameter_value min;
        union jackctl_parameter_value max;
        jackctl_parameter_get_range_constraint(jparam, &min, &max);
        if (param->dp == 1) {
          param->min = (double)min.ui;
          param->max = (double)max.ui;
        } else {
          param->min = (double)min.i;
          param->max = (double)max.i;
        }
      }
#ifdef DEBUG_JACK_PARAMS
      else g_print("%s range undefined, %d\n", param->name, def.i);
#endif
      param->value = lives_malloc(sizint);
      param->def = lives_malloc(sizint);
      if (param->dp == 1) {
        if (jackctl_parameter_is_set(jparam))
          set_int_param(param->value, val.ui);
        else
          set_int_param(param->value, def.ui);
        set_int_param(param->def, def.ui);
      } else {
        if (jackctl_parameter_is_set(jparam))
          set_int_param(param->value, val.i);
        else
          set_int_param(param->value, def.i);
        set_int_param(param->def, def.i);
      }
      param->dp = 0;
      break;
    case JackParamString:
      param->type = LIVES_PARAM_STRING;
      param->max = JACK_PARAM_STRING_MAX;
      param->min = -MIN(RFX_TEXT_MAGIC, JACK_PARAM_STRING_MAX);
      if (jackctl_parameter_is_set(jparam))
        param->value = lives_strdup(val.str);
      else
        param->value = lives_strdup(def.str);
      param->def = lives_strdup(def.str);
      break;
    case JackParamChar:
      param->type = LIVES_PARAM_STRING;
      param->max = 1.;
      if (jackctl_parameter_is_set(jparam))
        param->value = lives_strndup(&val.c, 1);
      else
        param->value = lives_strndup(&def.c, 1);
      param->def = lives_strndup(&def.c, 1);
      break;
    case JackParamBool:
      param->type = LIVES_PARAM_BOOL;
      param->value = lives_malloc(sizint);
      param->def = lives_malloc(sizint);
      if (jackctl_parameter_is_set(jparam))
        set_bool_param(param->value, (int)val.b);
      else
        set_bool_param(param->value, (int)def.b);
      set_bool_param(param->def, (int)def.b);
      break;
    default: break;
    }
    if (jackctl_parameter_has_enum_constraint(jparam)) {
      uint32_t nvals = jackctl_parameter_get_enum_constraints_count(jparam);
#ifdef DEBUG_JACK_PARAMS
      g_print("nvals is %d for %s\n", nvals, param->name);
#endif
      param->vlist_type = param->type;
      param->type = LIVES_PARAM_STRING_LIST;
      param->max = (double)nvals;
      param->min = 0.;
      for (int j = 0; j < nvals; j++) {
        void *vlval = NULL;
        union jackctl_parameter_value lval =
            jackctl_parameter_get_enum_constraint_value(jparam, j);
        param->list =
          lives_list_append(param->list,
                            lives_strdup(jackctl_parameter_get_enum_constraint_description
                                         (jparam, j)));
        switch (param->vlist_type) {
        case JackParamUInt:
          if (get_int_param(param->def) == lval.ui)
            set_int_param(param->def, j);
          if (get_int_param(param->value) == lval.ui)
            set_int_param(param->value, j);
          vlval = lives_malloc(sizint);
          set_int_param(vlval, lval.ui);
          break;
        case JackParamInt:
          if (get_int_param(param->def) == lval.i)
            set_int_param(param->def, j);
          if (get_int_param(param->value) == lval.i)
            set_int_param(param->value, j);
          vlval = lives_malloc(sizint);
          set_int_param(vlval, lval.i);
          break;
        case JackParamString:
          if (!lives_strcmp(param->def, lval.str)) {
            lives_free(param->def);
            param->def = lives_strdup(lval.str);
          }
          if (!lives_strcmp(param->value, lval.str)) {
            lives_free(param->value);
            param->value = lives_strdup(lval.str);
          }
          vlval = lives_strdup(lval.str);
          break;
        case JackParamChar:
          if (!lives_strncmp(param->def, &lval.c, 1)) {
            lives_free(param->def);
            param->def = lives_strndup(&lval.c, 1);
          }
          if (!lives_strncmp(param->value, &lval.c, 1)) {
            lives_free(param->value);
            param->value = lives_strndup(&lval.c, 1);
          }
          vlval = lives_strndup(&lval.c, 1);
          break;
        case JackParamBool:
          if (get_bool_param(param->def) == lval.b)
            set_bool_param(param->def, j);
          if (get_int_param(param->value) == lval.i)
            set_bool_param(param->value, j);
          vlval = lives_malloc(sizint);
          set_bool_param(vlval, lval.b);
          break;
        default: break;
        }
        param->vlist = lives_list_append(param->vlist, vlval);
      }
    }
    pcount++;
  }
  return rfx;
}


static boolean jack_params_edit(lives_rfx_t *rfx, boolean is_trans, boolean is_driver) {
  LiVESWidget *dialog, *pbox, *vbox, *label;
  LiVESResponseType resp;
  char *text;
  char *title = lives_strdup_printf(_("%s configuration for LiVES %s server"),
                                    is_driver ? _("Driver") : _("Server"),
                                    is_trans ? _("transport") : _("audio"));
  dialog = lives_standard_dialog_new(title, TRUE, is_driver ? (RFX_WINSIZE_H * 3) >> 1 : -1, (RFX_WINSIZE_V * 3) >> 1);
  pbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  vbox = lives_vbox_new(FALSE, 0);
  text = lives_strdup_printf(_("WARNING: Supplying incorrect %s settings may prevent "
                               "LiVES from\nstarting jack. "
                               "As an aid, LiVES will back up your current settings\n"
                               "and restore them in the event that there is a fatal error "
                               "with new the new configuration."), is_driver ? _("driver")
                             : _("server"));
  label = lives_standard_label_new(text);
  lives_free(text);
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
  if (!is_driver) {
    text = _("<b>NOTE: some values (e.g. --temporary, --sync) may be overridden by other configuration settings</b>");
    widget_opts.use_markup = TRUE;
    label = lives_standard_label_new(text);
    widget_opts.use_markup = FALSE;
    lives_free(text);
    lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);
  }
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pbox), EXTRA_WIDGET_KEY, (livespointer)vbox);
  make_param_box(LIVES_VBOX(pbox), rfx);
  resp = lives_dialog_run(LIVES_DIALOG(dialog));
  if (resp == LIVES_RESPONSE_OK) {
    lives_widget_destroy(dialog);
    return TRUE;
  }
  return FALSE;
}


static boolean driver_params_to_rfx(jackctl_driver_t *driver) {
  const JSList *dparams = jackctl_driver_get_parameters(driver);
  lives_rfx_t *rfx = jack_params_to_rfx(dparams, driver);
  boolean res = jack_params_edit(rfx, FALSE, TRUE);
  rfx_free(rfx);
  lives_free(rfx);
  return res;
}


static jackctl_driver_t *defdriver;
static LiVESList *new_slave_list = NULL;

static void set_def_driver(LiVESWidget * rb, jackctl_driver_t *driver) {
  defdriver = driver;
}


#ifndef JACK_V2
static void onif1(LiVESToggleButton * tb, LiVESWidget * bt) {
  int actv = GET_INT_DATA(bt, "actv");
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tb))) actv++;
  else actv--;
  if (!actv) lives_widget_set_sensitive(bt, FALSE);
  else lives_widget_set_sensitive(bt, TRUE);
  SET_INT_DATA(bt, "actv", actv);
}
#endif


static void add_slave_driver(LiVESWidget * rb, jackctl_driver_t *driver) {
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rb))) {
    new_slave_list = lives_list_append(new_slave_list, driver);
  } else {
    new_slave_list = lives_list_remove(new_slave_list, driver);
  }
}


static void config_driver(LiVESWidget * b, jackctl_driver_t *driver) {
  driver_params_to_rfx(driver);
}

// logs errtxt and returns FALSE, or checks if last_errmsg has text,
// if so, logs it, clears it, and returns FALSE otherwise just returns TRUE
// if errtxt starts with '#' then the message is printed as-is skipping the initial '#',
// otherwise we add timestamp and formatting
boolean jack_log_errmsg(jack_driver_t *jackd, const char *errtxt) {
  if (!jackd || (!errtxt && !*last_errmsg)) return TRUE;
  else {
    size_t offs = lives_strlen(jackd->status_msg);
    const char *errmsg = errtxt ? errtxt : last_errmsg;
    const char *isjack = "";
    char *tstamp;
    if (offs == STMSGLEN) {
      if (errmsg == last_errmsg) *last_errmsg = 0;
      return FALSE;
    }
    if (errmsg == last_errmsg) isjack = " JACKD:";
    tstamp = get_current_timestamp();
    if (!textwindow) tstwin = NULL;
    if (tstwin) {
      GET_PROC_THREAD_SELF(self);
      char *logmsg;
      uint64_t ostate = 0;
      if (self) ostate = lives_proc_thread_include_states(self, THRD_STATE_BUSY);
      if (*errmsg == '#') logmsg = lives_strdup(errmsg + 1);
      else logmsg = lives_strdup_printf("%s:%s %s\n", tstamp, isjack, errmsg);
      if (!widget_opts.use_markup) {
        char *xlog = lives_markup_escape_text(logmsg, -1);
        lives_text_buffer_insert_at_end(textbuf, xlog);
        lives_free(xlog);
      } else
        lives_text_buffer_insert_markup_at_end(textbuf, logmsg);
      lives_scrolled_window_scroll_to(LIVES_SCROLLED_WINDOW(tstwin->scrolledwindow), LIVES_POS_BOTTOM);
      if (is_fg_thread()) lives_widget_queue_draw(tstwin->dialog);
      lives_widget_context_update();
      for (int tt = 0; tt < 1000; tt++) {
        lives_nanosleep(LIVES_SHORT_SLEEP);
      }
      lives_scrolled_window_scroll_to(LIVES_SCROLLED_WINDOW(tstwin->scrolledwindow), LIVES_POS_BOTTOM);
      if (self && !(ostate & THRD_STATE_BUSY)) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
    } else {
      if (*errmsg == '#') {
        lives_snprintf(jackd->status_msg + offs, STMSGLEN - offs, "%s\n", errmsg + 1);
      } else lives_snprintf(jackd->status_msg + offs, STMSGLEN - offs, "%s:%s %s\n", tstamp, isjack, errmsg);
    }
    lives_free(tstamp);
    if (errmsg == last_errmsg) *last_errmsg = 0;
  }
  return FALSE;
}


static void add_test_textwin(jack_driver_t *jackd) {
  GET_PROC_THREAD_SELF(self);
  char *logmsg;
  char *title = lives_strdup(_("Jack configuration test"));
  uint64_t ostate = 0;
  if (self) ostate = lives_proc_thread_include_states(self, THRD_STATE_BUSY);
  textbuf = lives_text_buffer_new();
  tstwin = create_text_window(title, NULL, textbuf, FALSE);
  lives_free(title);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(tstwin->scrolledwindow),
                                   LIVES_POLICY_AUTOMATIC, LIVES_POLICY_ALWAYS);

  tstwin->button =
    lives_dialog_add_button_from_stock(LIVES_DIALOG(tstwin->dialog),
                                       LIVES_STOCK_CLOSE, _("_Close Window"),
                                       LIVES_RESPONSE_CANCEL);
  lives_widget_set_sensitive(tstwin->button, FALSE);
  lives_widget_show_all(tstwin->dialog);
  if (mainw->is_ready) pop_to_front(tstwin->dialog, NULL);
  logmsg = lives_strdup(_("Testing jack configuration..."));
  jack_log_errmsg(jackd, logmsg);
  lives_free(logmsg);
  ts_running = as_running = NULL;
  twins = as_started = ts_started = FALSE;
  jackserver = NULL;
  if (self && !(ostate & THRD_STATE_BUSY)) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
}


void jack_dump_metadata(void) {
  // seems not work...always returns -1 for ndesc
  jack_description_t *desc;
  uint32_t pcnt;
  jack_property_t *props;
  jack_client_t *client = NULL;
  jack_options_t options = JackNullOption | JackNoStartServer;
  int ndsc;
  jack_set_error_function(jack_error_func);
  if ((client = jack_client_open("jack-property", options, NULL)) == 0) {
    g_print("Cannot connect to JACK server\n");
  }  ndsc = jack_get_all_properties(&desc);
  g_print("GOT %d props\n%s", ndsc, last_errmsg);
  for (int i = 0; i < ndsc; i++) {
    pcnt = desc[i].property_cnt;
    props = desc[i].properties;
    g_print("JACK PROPERTIES for %lu\n", desc[i].subject);
    for (int j = 0; j < pcnt; j++) {
      if (!props[j].type || !*props[j].type) {
        g_print("%s = %s\n", props[j].key, props[j].data);
      } else {
        g_print("%s has type %s\n", props[j].key, props[j].type);
	// *INDENT-OFF*
      }}}
  // *INDENT-OFF*
  jack_client_close (client);
}


void show_jack_status(LiVESButton * button, livespointer is_transp) {
  int is_trans = LIVES_POINTER_TO_INT(is_transp);
  text_window *textwindow;
  char *title = NULL, *text, *tmp = NULL;
  if (is_trans == 3) {
    title = _("Jack Status Log");
  }
  if (is_trans == 2) {
    title = _("Jack Startup Log");
    lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  }
  if (is_trans == 1) {
    title = _("Status for jack transport client");
  }
  if (is_trans == 1 || is_trans == 3 || (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)) {
    if (mainw->jackd_trans) {
      if (1 || *mainw->jackd_trans->status_msg == '*') text = lives_strdup(mainw->jackd_trans->status_msg);
      else text = lives_markup_escape_text(mainw->jackd_trans->status_msg, -1);
    }
    else text = lives_strdup(_("Jack transport client was not started"));
    tmp = text;
  }
  if (is_trans != 1) {
    char *txt1, *txt2;
    if (mainw->jackd) {
      if (1 || *mainw->jackd->status_msg == '*') txt1 = lives_strdup(mainw->jackd->status_msg + 1);
      else txt1 = lives_markup_escape_text(mainw->jackd->status_msg, -1);
    }
    else txt1 = lives_strdup(_("Jack audio writer client was not started"));
    if (mainw->jackd_read) {
      if (1 || *mainw->jackd_read->status_msg == '*') txt2 = lives_strdup(mainw->jackd_read->status_msg + 1);
      else txt2 = lives_markup_escape_text(mainw->jackd_read->status_msg, -1);
    }
    else txt2 = lives_strdup(_("Jack audio reader client was not started"));

    if (tmp) {
      text = lives_strdup_printf("%s\n\n%s\n%s", tmp, txt1, txt2);
      lives_free(tmp);
    }
    else text = lives_strdup_printf("%s\n\n%s", txt1, txt2);

    lives_free(txt1); lives_free(txt2);

    if (!is_trans) title = _("Status for jack audio write and audio read clients");
  }
  //widget_opts.use_markup = TRUE;
  textwindow = create_text_window(title, text, NULL, TRUE);
  //widget_opts.use_markup = FALSE;
  lives_free(title); lives_free(text);
  if (is_trans == 3) return;

  lives_dialog_run(LIVES_DIALOG(textwindow->dialog));
  if (is_trans == 2) {
    lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(button)));
  }
}


static jackctl_driver_t *get_def_drivers(const JSList *drivers, LiVESList **slvlist, int type) {
  // type is 0. 1 (startup/prefs), 2, 3 (no startup error), 4, 5 (lives_jack_init)
  jackctl_driver_t *driver = NULL;
  JSList *xdrivers = (JSList *)drivers;
  LiVESList *mlist = NULL;
  LiVESList *slist = NULL;
  LiVESList *list;
  boolean is_trans = !(type & 1);
  char *title = lives_strdup_printf(_("Driver selection for LiVES %s server"), is_trans ? _("transport") : _("audio"));
  boolean is_setup = !mainw->is_ready;
  LiVESWidget *dialog = lives_standard_dialog_new(title, !(is_setup), -1, -1);
  LiVESWidget *dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));
  LiVESWidget *vbox = lives_vbox_new(FALSE, 0), *hbox, *rb, *cb, *button;
  LiVESWidget *layout = lives_layout_new(LIVES_BOX(vbox));
  LiVESWidget *scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, vbox);
  LiVESWidget *cancelbutton, *okbutton, *logbutton = NULL;
  LiVESSList *rb_group = NULL;
  char *msg, *msg2 = NULL, *tmp = NULL, *tmp2 = NULL, *tmp3 = NULL, *tmp4 = NULL, *tmp5 = NULL;
  char *jack1warn =
#ifdef JACK_V2
    lives_strdup("");
#else
  lives_strdup(_("\nNOTE: JACK *_1_* DOES NOT PROVIDE INFORMATION ABOUT WHICH DRIVERS "
		 "ARE MASTER\nAND WHICH ARE SLAVES !\n"
		 "PLEASE TAKE CARE THAT EXACTLY ONE MASTER DRIVER IS SELECTED "
		 "FROM THE LIST BELOW\nThe radio buttons may be used to select the Master driver, "
		 "and the check buttons to select additional Slaves."));
#endif
  if (is_setup) {
    char *linktxt = "";
    if (is_trans && prefs->jack_srv_dup) {
      linktxt = lives_strdup(_("The values entered here will also be applied to the audio client.\n"
			       "(Setting separate values for transport and audio clients is possible\n"
			       "but must done from within the application Preferences)"));
    }
    msg = lives_strdup_printf(_("LiVES failed to connect the %s client to the %s, "
				"so we will try to start %s instead.\n"
				" >>>> Since this is the first time connecting to a new server, you need to "
				"tell me which driver to use.\n%s\n"
				"Please select exactly ONE of the 'Master' drivers below, and optionally "
				"any number of 'Slaves'\n%s"),
                              is_trans ? _("transport") : _("audio"),
                              // ...connect to the
                              is_trans ? (*future_prefs->jack_tserver_cname
                                          ? (tmp3 = lives_strdup_printf("server named '<b>%s</b>'",
									(tmp4 = lives_markup_escape_text(prefs->jack_tserver_cname, -1))))
                                          : _("default server"))
                              : (*future_prefs->jack_aserver_cname
                                 ? (tmp3 = lives_strdup_printf("server named '<b>%s</b>'",
							       (tmp4 = lives_markup_escape_text(future_prefs->jack_aserver_cname, -1))))
				 : _("default server")),
                              // ..to start
                              is_trans ? (!*future_prefs->jack_tserver_sname ? _("the default server")
                                          : (!lives_strcmp(future_prefs->jack_tserver_cname, future_prefs->jack_tserver_sname)
					     ? "it" : (tmp = lives_strdup_printf("the server named '%s'",
										 (tmp5 = lives_markup_escape_text
										  (future_prefs->jack_tserver_sname, -1))))))
                              //
                              : (!*future_prefs->jack_aserver_sname ? _("the default server")
                                 : (!lives_strcmp(future_prefs->jack_aserver_cname, future_prefs->jack_aserver_sname)
                                    ? "it" : (tmp = lives_strdup_printf("the server named '%s'",
									(tmp5 = lives_markup_escape_text
									 (future_prefs->jack_aserver_sname, -1)))))), linktxt, jack1warn);

    if (*linktxt) lives_free(linktxt);

    msg2 = lives_strdup_printf(_("If preferred, you may start the %s manually, "
                                 "before clicking on 'Retry Connection' to try again."),
                               is_trans ? (!*future_prefs->jack_tserver_cname ? _("default server")
                                           : (tmp2 = lives_strdup_printf(_("server named '%s'"), future_prefs->jack_tserver_cname)))
                               : (!*future_prefs->jack_aserver_cname ? _("default server")
                                  : (tmp2 = lives_strdup_printf(_("server named '%s'"), future_prefs->jack_aserver_cname))));
  } else msg = lives_strdup_printf("%s%s",
				   _("WARNING: Supplying incorrect driver settings may "
				     "prevent LiVES from\nstarting jack. "
				     "As an aid, LiVES will back up your current settings\n"
				     "and restore them in the event that there is a fatal error "
				     "with new settings."), jack1warn);
  lives_freep((void **)&tmp); lives_freep((void **)&tmp2); lives_freep((void **)&tmp3);
  lives_freep((void **)&tmp4); lives_freep((void **)&tmp5);
  lives_free(jack1warn);
  widget_opts.use_markup = TRUE;
  lives_layout_add_label(LIVES_LAYOUT(layout), msg, TRUE);
  widget_opts.use_markup = FALSE;
  lives_free(msg);
  if (msg2) {
    lives_layout_add_row(LIVES_LAYOUT(layout));
    lives_layout_add_label(LIVES_LAYOUT(layout), msg2, TRUE);
    lives_free(msg2);
  }
  lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

  while (xdrivers) {
    driver = (jackctl_driver_t *)xdrivers->data;
#ifdef JACK_V2
    if (jackctl_driver_get_type(driver) == JackMaster) {
      mlist = lives_list_append(mlist, driver);
    } else {
      slist = lives_list_append(slist, driver);
    }
#else
    slist = lives_list_append(slist, driver);
#endif
    xdrivers = jack_slist_next(xdrivers);
  }

#ifdef JACK_V2
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Master driver:"), FALSE);
  if (mlist) defdriver = mlist->data;
  else defdriver = NULL;
  for (list = mlist; list; list = list->next) {
    const char *dname = jackctl_driver_get_name((jackctl_driver_t *)list->data);
    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    add_fill_to_box(LIVES_BOX(hbox));
    rb = lives_standard_radio_button_new(dname, &rb_group, LIVES_BOX(hbox), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(rb), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(set_def_driver), list->data);
    if (mainw->is_ready) {
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      button = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
               _("Configure"), -1, DEF_BUTTON_HEIGHT >> 1,
               LIVES_BOX(hbox), TRUE, NULL);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(config_driver), list->data);

      toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(rb), button, FALSE);
    }
  }

  add_hsep_to_box(LIVES_BOX(vbox));
  layout = lives_layout_new(LIVES_BOX(vbox));
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Slave drivers:"), FALSE);
#else
  if (slist) defdriver = slist->data;
  lives_layout_add_label(LIVES_LAYOUT(layout), _("Available drivers:"), FALSE);
#endif

  for (list = slist; list; list = list->next) {
    const char *dname = jackctl_driver_get_name((jackctl_driver_t *)list->data);
    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    add_fill_to_box(LIVES_BOX(hbox));
#ifndef JACK_V2
    rb = lives_standard_radio_button_new(_("Master"), &rb_group, LIVES_BOX(hbox), NULL);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(rb), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(set_def_driver), list->data);
#endif
    cb = lives_standard_check_button_new(dname, FALSE, LIVES_BOX(hbox), NULL);
    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(add_slave_driver), list->data);
    if (mainw->is_ready) {
      hbox = lives_layout_hbox_new(LIVES_LAYOUT(layout));
      button = lives_standard_button_new_from_stock_full(LIVES_STOCK_PREFERENCES,
               _("Configure"), -1, DEF_BUTTON_HEIGHT >> 1,
               LIVES_BOX(hbox), TRUE, NULL);
      lives_signal_sync_connect(LIVES_GUI_OBJECT(button), LIVES_WIDGET_CLICKED_SIGNAL,
                                LIVES_GUI_CALLBACK(config_driver), list->data);
#ifdef JACK_V2
      toggle_sets_sensitive(LIVES_TOGGLE_BUTTON(cb), button, FALSE);
#else
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(cb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(onif1), button);
      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(rb), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(onif1), button);
      if (list != slist) lives_widget_set_sensitive(button, FALSE);
      else lives_widget_object_set_data(LIVES_WIDGET_OBJECT(button), "actv", LIVES_INT_TO_POINTER(1));
#endif
    }
  }

  if (slist) lives_list_free(slist);
  if (!is_setup) lives_list_free(new_slave_list);
  slist = new_slave_list;
  new_slave_list = NULL;

  if (!mainw->is_ready) {
    LiVESWidget *bbox = lives_dialog_get_action_area(LIVES_DIALOG(dialog));

    if (type != 2 && type != 3) {
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
							LIVES_STOCK_GO_BACK, _("Exit LiVES"), LIVES_RESPONSE_RESET);
      lives_window_add_escape(LIVES_WINDOW(dialog), cancelbutton);

      lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH, _("_Retry Connection"),
					 LIVES_RESPONSE_RETRY);

      logbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_REFRESH, _("View Status _Log"),
						   LIVES_RESPONSE_SHOW_DETAILS);

      okbutton =lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_OK, _("_Start Server"),
						   LIVES_RESPONSE_OK);
      lives_button_grab_default_special(okbutton);
      lives_button_box_set_layout(LIVES_BUTTON_BOX(bbox), LIVES_BUTTONBOX_SPREAD);
	pop_to_front(dialog, NULL);
    }
    else {
      cancelbutton = lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog),
							LIVES_STOCK_GO_BACK, LIVES_STOCK_LABEL_BACK, LIVES_RESPONSE_CANCEL);

      okbutton =lives_dialog_add_button_from_stock(LIVES_DIALOG(dialog), LIVES_STOCK_GO_FORWARD, _("Try New Config"),
						   LIVES_RESPONSE_ACCEPT);
    }
  }

  while (1) {
    LiVESResponseType response = lives_dialog_run(LIVES_DIALOG(dialog));
    if (!is_setup) return NULL;
    if (response == LIVES_RESPONSE_SHOW_DETAILS) {
      show_jack_status(LIVES_BUTTON(logbutton), LIVES_INT_TO_POINTER(2));
      continue;
    }
    lives_widget_destroy(dialog);
    lives_widget_context_update();
    if (response == LIVES_RESPONSE_ACCEPT) return driver;
    if (response == LIVES_RESPONSE_RESET || response == LIVES_RESPONSE_CANCEL) {
      mainw->cancelled = CANCEL_USER;
      return NULL;
    }
    if (response == LIVES_RESPONSE_RETRY) {
      if (slist) {
        lives_list_free(slist);
        slist = NULL;
      }
      defdriver = NULL;
    } else {
#ifndef JACK_V2
      if (slist) slist = lives_list_remove(slist, defdriver);
#endif
    }
    break;
  }
  if (slvlist) *slvlist = slist;
  return defdriver;
}

void jack_srv_startup_config(LiVESWidget *b, livespointer type_data) {
  boolean is_trans = LIVES_POINTER_TO_INT(type_data);
  // button callback from Prefs window
  // show the same config window as during setup, but with some extra options
  do_jack_config(0, is_trans);
}


boolean jack_drivers_config(LiVESWidget *b, livespointer ptype) {
  // type is 0. 1 (startup/prefs), 2, 3 (no_startup error)
  const JSList *drivers;
  LiVESList *slvlist = NULL;
  jackctl_driver_t *driver = NULL;

  int type = LIVES_POINTER_TO_INT(ptype);
  if (type & 1) {
    drivers = prefs->jack_adrivers;
    slvlist = prefs->jack_tslaves;
  }
  else {
    drivers = prefs->jack_tdrivers;
    slvlist = prefs->jack_tslaves;
  }
  if (type > 1) lives_widget_hide(lives_widget_get_toplevel(LIVES_WIDGET(b)));
  driver = get_def_drivers(drivers, &slvlist, type);
  if (type > 1)  lives_widget_show(lives_widget_get_toplevel(LIVES_WIDGET(b)));
  if (!driver) return FALSE;
  else {
    const char *dname = jackctl_driver_get_name(driver);
#ifdef ENABLE_JACK_TRANSPORT
    const char *asname, *tsname;
    boolean all_equal = FALSE;
    if (*future_prefs->jack_aserver_sname) asname = future_prefs->jack_aserver_sname;
    else asname = prefs->jack_def_server_name;
    if (*future_prefs->jack_tserver_sname) tsname = future_prefs->jack_tserver_sname;
    else tsname = prefs->jack_def_server_name;
    if (!lives_strcmp(asname, tsname)) all_equal = TRUE;
#endif
    if (type & 1) {
      //prefs->jack_adriver = lives_strdup_free(prefs->jack_adriver, dname);
      future_prefs->jack_adriver = lives_strdup_free(future_prefs->jack_adriver, dname);
#ifdef ENABLE_JACK_TRANSPORT
      if (prefs->jack_srv_dup) {
	//prefs->jack_tdriver = lives_strdup(prefs->jack_tdriver, driver);
	future_prefs->jack_tdriver = lives_strdup_free(future_prefs->jack_tdriver, dname);
      }
#endif
      if (future_prefs->jack_aslaves && future_prefs->jack_aslaves != future_prefs->jack_tslaves) {
	lives_list_free(future_prefs->jack_aslaves);
	future_prefs->jack_aslaves = slvlist;
#ifdef ENABLE_JACK_TRANSPORT
	if (all_equal) future_prefs->jack_aslaves = slvlist;
#endif
      }
    }
#ifdef ENABLE_JACK_TRANSPORT
    else {
      //prefs->jack_tdriver = lives_strdup_free(prefs->jack_tdriver, dname);
      future_prefs->jack_tdriver = lives_strdup_free(future_prefs->jack_tdriver, dname);

      if (prefs->jack_srv_dup) {
	//prefs->jack_adriver = lives_strdup(prefs->jack_adriver, driver);
	future_prefs->jack_adriver = lives_strdup_free(future_prefs->jack_adriver, dname);
      }

      if (future_prefs->jack_tslaves && future_prefs->jack_tslaves != future_prefs->jack_aslaves) {
	lives_list_free(future_prefs->jack_tslaves);
	future_prefs->jack_tslaves = slvlist;
	if (all_equal) future_prefs->jack_tslaves = slvlist;
      }
    }
#endif
    return TRUE;
  }
}


void jack_server_config(LiVESWidget *b, lives_rfx_t *rfx) {
  jack_params_edit(rfx, rfx->source == prefs->jack_tserver, FALSE);
}


static const char *get_type_name(lives_jack_client_type type) {
  if (type == JACK_CLIENT_TYPE_TRANSPORT) return "transport";
  if (type == JACK_CLIENT_TYPE_AUDIO_WRITER) return "audio writer";
  if (type == JACK_CLIENT_TYPE_AUDIO_READER) return "audio reader";
  return "??";
}


char *jack_parse_script(const char *fname) {
  char *line = lives_fread_line(fname), *retv;
  if (line) {
    int i;
    for (i = 0; line[i] && line[i + 1]; i++) if (line[i] ==  '-') {
	if (line[i + 1] == 'd' || line[i + 1] == 'n'
	    || (line[i + 1] == '-' && (line[i + 2] == 'n'
				       || line[i + 2] == 'd'))) break;
	i++;
      }
    if (!line[i]) {
      lives_free(line);
      return NULL;
    }
    for (; line[i]; i++) {
      if (line[i] != '-') continue;
      //i++;
      if (line[++i] == 'd') break;
      if (!lives_strncmp(&line[i], "-driver", 7)
	  && (line[i + 7] == ' ' || line[i + 7] == '=')) break;
      if (line[i] == 'n' || (!lives_strncmp(&line[i], "-name", 5)
			     && (line[i + 5] == ' ' || line[i + 5] == '='))) {
	int start = -1, end = -1, done = 0;
	char quote = 0;
	if (line[i] == '-') i += 5;
	for (i++; line[i] && !done; i++) {
	  switch (line[i]) {
	  case ' ': if (start != -1 && !quote) done = 1; break;
	  case '\'': case '"':
	    if (!quote || quote == line[i]) {
	      quote = line[i] - quote;
	      break;
	    }
	  default:
	    if (start == -1) start = i;
	    end = i;
	    break;
	  }
	}
	retv = (end <= start || start == -1 ? NULL :
		lives_strndup(&line[start], end - start + 1));
	lives_free(line);
	return retv;
      }
    }
  }
  return NULL;
}


boolean jack_get_cfg_file(boolean is_trans, char **pserver_cfgx) {
  char *server_cfgx = NULL;
  boolean cfg_exists = FALSE;
  if (is_trans) server_cfgx = lives_strdup(future_prefs->jack_tserver_cfg);
  else server_cfgx = lives_strdup(future_prefs->jack_aserver_cfg);
  if (!server_cfgx || !*server_cfgx) {
    if (server_cfgx) lives_free(server_cfgx);
    server_cfgx = lives_build_filename(capable->home_dir, "." JACKD_RC_NAME, NULL);
    if (!lives_file_test(server_cfgx, LIVES_FILE_TEST_EXISTS)) {
      char *server_cfgx2 = lives_find_program_in_path("autojack");
      if (server_cfgx2) {
        lives_free(server_cfgx);
        server_cfgx = server_cfgx2;
        cfg_exists = TRUE;
      }
      else {
	server_cfgx2 = lives_build_filename("etc", JACKD_RC_NAME, NULL);
	if (lives_file_test(server_cfgx2, LIVES_FILE_TEST_EXISTS)) {
	  lives_free(server_cfgx);
	  server_cfgx = server_cfgx2;
	  cfg_exists = TRUE;
	} else lives_free(server_cfgx2);
      }
    } else cfg_exists = TRUE;
  } else {
    if (lives_file_test(server_cfgx, LIVES_FILE_TEST_EXISTS))
      cfg_exists = TRUE;
  }
  if (pserver_cfgx) *pserver_cfgx = server_cfgx;
  return cfg_exists;
}


static void err_discon(void) {
  const char *type_name;
  char *logmsg;
  if (mainw->jackd) {
    if (mainw->jackd->client) {
      type_name = get_type_name(JACK_CLIENT_TYPE_AUDIO_WRITER);
      logmsg = lives_strdup_printf(_("Disconnecting %s\n"), type_name);
      jack_log_errmsg(mainw->jackd, logmsg);
      lives_free(logmsg);
      jack_client_close(mainw->jackd->client);
      mainw->jackd->client = NULL;
    }
    if (*future_prefs->jack_aserver_cfg && as_scripted) {
      char *server_name;
      boolean still_running = FALSE;
      jack_options_t options = JackNullOption | JackNoStartServer;

      if (aserver_pid) {
	// this will fail for scripts, because the actual command gets run by the shell in a new sid / pgid
	if (!lives_kill(aserver_pid, 0)) lives_killpg(aserver_pid, SIGKILL);
	aserver_pid = 0;
      }

      // try again to connect, without starting it, if the script is gone we should fail
      server_name = jack_parse_script(future_prefs->jack_aserver_cfg);
      mainw->jackd->client = jack_client_open("conx-test", options, NULL, server_name);
      lives_free(server_name);
      if (mainw->jackd->client) {
	still_running = TRUE;
	jack_client_close(mainw->jackd->client);
      }
      if (still_running) {
	logmsg = _("#\n<b>LiVES was unable to stop the jack audio config script. You may wish to terminate the script manually "
		   "now if you intend to continue testing.</b>\n\n");
	widget_opts.use_markup = TRUE;
	jack_log_errmsg(mainw->jackd, logmsg);
	widget_opts.use_markup = FALSE;
	lives_free(logmsg);
      }
    }
  }

  if (mainw->jackd_trans) {
    if (mainw->jackd_trans->client) {
      type_name = get_type_name(JACK_CLIENT_TYPE_TRANSPORT);
      logmsg = lives_strdup_printf(_("Disconnecting %s\n"), type_name);
      jack_log_errmsg(mainw->jackd_trans, logmsg);
      lives_free(logmsg);
      jack_client_close(mainw->jackd_trans->client);
      mainw->jackd_trans->client = NULL;
    }

    if (*future_prefs->jack_tserver_cfg && ts_scripted) {
      char *server_name;
      boolean still_running = FALSE;
      jack_options_t options = JackNullOption | JackNoStartServer;

      if (tserver_pid) {
	// this will fail for scripts, because the actual command gets run by the shell in a new sid / pgid
	if (!lives_kill(tserver_pid, 0)) lives_killpg(tserver_pid, SIGKILL);
	tserver_pid = 0;
      }

      // try again to connect, without starting it, if the script is gone we should fail
      server_name = jack_parse_script(future_prefs->jack_tserver_cfg);
      mainw->jackd_trans->client = jack_client_open("conx-test", options, NULL, server_name);
      lives_free(server_name);
      if (mainw->jackd_trans->client) {
	still_running = TRUE;
	jack_client_close(mainw->jackd_trans->client);
      }
      if (still_running) {
	logmsg = _("#\n<b>LiVES was unable to stop the jack transport config script. You may wish to terminate the script manually "
		   "now if you intend to continue testing.</b>\n\n");
	widget_opts.use_markup = TRUE;
	jack_log_errmsg(mainw->jackd_trans, logmsg);
	widget_opts.use_markup = FALSE;
	lives_free(logmsg);
      }
    }
  }
}

static void finish_test(jack_driver_t *jackd, boolean success, boolean is_trans, boolean is_reader) {
  // if servers started, make them temp always, so then we need to keep clients alive until
  // after dealing with audio reader (+audio_writer if this is the transport server we started,
  // and audio connect server is the same)
  char *logmsg;
 boolean close_client = TRUE;

  // if we only connected, we can close the client
  if (jackserver) {
    // if this is transport client and we started a server, we can close it if either:
    // audio client use different server names for connect and startup (or it is not set to startup)
    // - if we are setup mode, keep all clients running as if we succeed they will be our proper clients
    // unless we failed of course, then we can close the client and maybe killpg a script
    if (prefs->startup_phase) close_client = !success;
    else {
      if (is_trans) {
	if (!*future_prefs->jack_tserver_cfg) {
	  // if aserver will connect to this server, we are fine, but we need to keep the tclient alive
	  if (!lives_strcmp(future_prefs->jack_tserver_sname, future_prefs->jack_aserver_cname)
	      || !lives_strcmp(future_prefs->jack_tserver_sname, future_prefs->jack_aserver_sname))
	    close_client = FALSE;
	}
	else {
	  if (!lives_strcmp(future_prefs->jack_tserver_cname, future_prefs->jack_aserver_cname) ||
			    !lives_strcmp(future_prefs->jack_tserver_cname, future_prefs->jack_aserver_sname))
	    close_client = FALSE;
	  // *INDENT-OFF*
	}}
      else {
	// for audio server, we need to keep the server alive until reader has connected
	if (!is_reader) close_client = FALSE;
      }}}
  // *INDENT-ON*

  if (!success) close_client = TRUE;

  if (close_client || (prefs->startup_phase && is_reader)) {
    if (!prefs->startup_phase) {
      const char *type_name;

      if (is_trans) type_name = get_type_name(JACK_CLIENT_TYPE_TRANSPORT);
      else {
        if (is_reader)
          type_name = get_type_name(JACK_CLIENT_TYPE_AUDIO_READER);
        else
          type_name = get_type_name(JACK_CLIENT_TYPE_AUDIO_WRITER);
      }

      logmsg = lives_strdup_printf(_("Disconnecting %s\n"), type_name);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);

      jack_client_close(jackd->client);
      jackd->client = NULL;
    }
    if (is_reader || !success) {
      // if we are closing the reader client we can now close the writer client
      // and if writer failed, we should close trans
      if (!prefs->startup_phase || !success) {
        err_discon();
      }
      logmsg = _("Testing complete.\n");
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
      if (!success)
        logmsg = lives_strdup_printf("#<big><b>%s</b></big>", _(" - JACK CONFIGURATION TEST FAILED - "
                                     "please review the log above,\n"
                                     "and check if the startup configuration needs adjusting.\n"));
      else
        logmsg = lives_strdup_printf("#<big><b>%s</b></big>", _(" - JACK CONFIGURATION TEST COMPLETED SUCCESSFULLY -\n"));
      widget_opts.use_markup = TRUE;
      jack_log_errmsg(jackd, logmsg);
      widget_opts.use_markup = FALSE;
      lives_free(logmsg);
      tstwin = NULL;
    }
  }
  // else leave client running until audio_reader has connected
}


// connect to, or (optionally) start a jack server
// for audio clients this should be called with is_trans = FALSE, and then followed with
// jack_create_client_writer and / or jack_create_client_reader
// function should be called at startup iff JACK_OPTS_ENABLE_TCLIENT is set (for transport client)
// and whenever this is enabled in Prefs.
// for audio clients, should be called at startup if prefs->audio_player == AUD_PLAYER_JACK
// and whenever the audio player is set to this value in Prefs.
// error conditions:
// - connection fails and option to startup not set
// -- resolutions: allow user to start sever manually and retry connection,
//                 let user change settings so server startup can be tried
//                 abort from LiVES
//
// - server startup fails, no backup config
// -- resolutions: on fatal error, disable auto startup, abort
// --              on less severe errors, allow user to disable auto start, and start server manually
// --              allow user to try with alternate server / driver config
//
// - server startup fails, backup config available
// -- resolutions: on fatal error, restore previous (working). abort
// --              on less sever errors, allow choice of reverting settings or altering current ones
boolean lives_jack_init(lives_jack_client_type client_type, jack_driver_t *jackd) {
  GET_PROC_THREAD_SELF(self);
  lives_proc_thread_t defer_lpt;
  jack_options_t options = JackNullOption |  JackServerName;
  jack_status_t status;
  jackctl_driver_t *driver = NULL;
  const JSList *drivers;
  const JSList *params;
  LiVESList *slave_list;
  lives_pid_t sc_pid = 0;

  char *server_name;
  char *client_name = NULL;
  char *com = NULL;
  char *logmsg, *logmsg2, *tmp, *tmp2;
  const char *scrfile;

  const char *driver_name;
  const char *type_name = get_type_name(client_type);
  const char *defservname;

  boolean set_server_sync = FALSE;
  boolean set_server_temp = FALSE;
  boolean is_trans = FALSE;
  boolean is_test = FALSE;
  boolean is_reader = FALSE;
  boolean was_started = FALSE;
  boolean needs_sigs = FALSE;
  boolean test_ret = FALSE;
  boolean script_tried = FALSE;

#ifndef JACK_V2
  int jackver = 1;
#else
  int jackver = 2;
#endif

  int con_attempts = 0;

  if (!is_inited) {
    jack_set_error_function(jack_error_func);
    jackctl_setup_signals(0);
    is_inited = TRUE;
  }

  if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) {
    is_test = TRUE;
    if (!tstwin) add_test_textwin(jackd);
  }

  *last_errmsg = 0;

  if (!*prefs->jack_def_server_name) {
    defservname = getenv(JACK_DEFAULT_SERVER);
    if (defservname)
      lives_snprintf(prefs->jack_def_server_name, JACK_PARAM_STRING_MAX,
                     "%s", defservname);
    if (!*prefs->jack_def_server_name) lives_snprintf(prefs->jack_def_server_name, JACK_PARAM_STRING_MAX,
          "%s", JACK_DEFAULT_SERVER_NAME);
  }

  defservname = prefs->jack_def_server_name;

  if (client_type == JACK_CLIENT_TYPE_TRANSPORT) {
    is_trans = TRUE;
    jackd = mainw->jackd_trans;
  } else if (client_type == JACK_CLIENT_TYPE_AUDIO_READER) is_reader = TRUE;

  if (!jackd) jackd = (jack_driver_t *)lives_calloc(sizeof(jack_driver_t), 1);

  jackd->client_type = client_type;

  if (is_trans) {
    if (!mainw->jackd_trans) mainw->jackd_trans = jackd;
    if (jackd->client) {
      if (ts_running) goto ret_success;
    }
    if (!jackd->client_name) {
      client_name = jackd->client_name = lives_strdup_printf("LiVES:transport");
    } else client_name = jackd->client_name;

    if (*future_prefs->jack_tserver_cname) server_name = lives_strdup(future_prefs->jack_tserver_cname);
    else server_name = lives_strdup(defservname);

    if (is_test) {
      logmsg = lives_strdup_printf("Trying %s connection to server '%s'",
                                   type_name, server_name);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
    }
    // if we know for sure that the audio client is connected to the target server
    // then we will just connect
    if (as_running && !lives_strcmp(as_running, server_name)) {
      logmsg = lives_strdup_printf(_("#\nserver named '%s' was already %s, new connection attempt by %s client should succeed\n\n"),
                                   server_name, as_started ? _("created by us") : _("successfully connected to"),
                                   type_name);
      ts_running = server_name;
      ts_started = as_started;
      was_started = TRUE;
      twins = TRUE;
      goto do_connect;
    }
    twins = FALSE;
  } else {
    if (!jackd->client_name) {
      if (is_reader)
        client_name = jackd->client_name = lives_strdup("LiVES_in");
      else
        client_name = jackd->client_name = lives_strdup("LiVES_out");
    } else client_name = jackd->client_name;
    if (as_running) was_started = TRUE;
    if (*future_prefs->jack_aserver_cname) server_name = lives_strdup(future_prefs->jack_aserver_cname);
    else server_name = lives_strdup(defservname);
    if (is_test) {
      logmsg = lives_strdup_printf("Trying %s connection to server '%s'",
                                   type_name, server_name);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
    }
    if (!as_running) {
      if (ts_running && !lives_strcmp(ts_running, server_name)) {
        as_running = server_name;
        as_started = ts_started;
        twins = TRUE;
      }
    }
    if (as_running) {
      // e.g reader startup after writer started server
      logmsg = lives_strdup_printf(_("#\nserver named '%s' was already %s, new connection attempt by %s client should succeed\n\n"),
                                   server_name, as_started ? _("created by us") : _("successfully connected to"),
                                   type_name);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
      was_started = TRUE;
      goto do_connect;
    }
    twins = FALSE;
  }

retry_connect:
  jackserver = NULL;

  if (is_trans) {
    // try to connect, then if we fail, start the server
    // if server name is NULL, then use 'default' or $JACK_DEFAULT_SERVER
    jack_options_t xoptions = (jack_options_t)((int)options | (int)JackNoStartServer);

    mainw->crash_possible = 1;
    defer_lpt = lives_hook_append(NULL, THREAD_EXIT_HOOK, 0,
                                  defer_sigint_cb, LIVES_INT_TO_POINTER(mainw->crash_possible));

    if (!mainw->signals_deferred) {
      // try to handle crashes in jack_client_open()
      set_signal_handlers((lives_sigfunc_t)defer_sigint);
      needs_sigs = TRUE;
    }

    logmsg = lives_strdup_printf(_("%s client will try to connect to server named '%s'"),
                                 type_name, server_name);
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    jackd->client = jack_client_open(client_name, xoptions, &status, server_name);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

    lives_hook_remove(defer_lpt);

    if (needs_sigs) {
      set_signal_handlers((lives_sigfunc_t)catch_sigint);
      mainw->crash_possible = 0;
    }

    if (jackd->client) {
      ts_running = server_name;
      logmsg = lives_strdup_printf(_("%s client '%s' <b>successfully connected</b> to running jack v%d server named '%s'"),
                                   type_name, (tmp = lives_markup_escape_text(client_name, -1)), jackver,
                                   (tmp2 = lives_markup_escape_text(server_name, -1)));
      lives_free(tmp); lives_free(tmp2);
      widget_opts.use_markup = TRUE;
      jack_log_errmsg(jackd, logmsg);
      widget_opts.use_markup = FALSE;
      lives_free(logmsg);
      was_started = TRUE;
      goto connect_done;
    }
    jack_log_errmsg(jackd, NULL);
  } else {
    // try to connect, then if we fail, start the server
    // if server name is NULL, then use 'default' or $JACK_DEFAULT_SERVER
    jack_options_t xoptions = (jack_options_t)((int)options | (int)JackNoStartServer);
    mainw->crash_possible = 2;
    defer_lpt = lives_hook_append(NULL, THREAD_EXIT_HOOK, 0,
                                  defer_sigint_cb, LIVES_INT_TO_POINTER(mainw->crash_possible));
    if (!mainw->signals_deferred) {
      // try to handle crashes in jack_client_open()
      set_signal_handlers((lives_sigfunc_t)defer_sigint);
      needs_sigs = TRUE;
    }

    logmsg = lives_strdup_printf(_("%s client will try to connect to server named '%s'"),
                                 type_name, server_name);
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    jackd->client = jack_client_open(client_name, xoptions, &status, server_name);
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    lives_hook_remove(defer_lpt);

    if (needs_sigs) {
      set_signal_handlers((lives_sigfunc_t)catch_sigint);
      mainw->crash_possible = 0;
    }
    if (jackd->client) {
      as_running = server_name;
      logmsg = lives_strdup_printf(_("%s client '%s' <b>successfully connected</b> to running jack v%d server named '%s'"),
                                   type_name, (tmp = lives_markup_escape_text(client_name, -1)), jackver,
                                   (tmp2 = lives_markup_escape_text(server_name, -1)));
      lives_free(tmp); lives_free(tmp2);
      widget_opts.use_markup = TRUE;
      jack_log_errmsg(jackd, logmsg);
      widget_opts.use_markup = FALSE;
      lives_free(logmsg);
      was_started = TRUE;
      goto connect_done;
    }
    jack_log_errmsg(jackd, NULL);
  }

  if ((is_trans && !(future_prefs->jack_opts & JACK_OPTS_START_TSERVER)
       && !*future_prefs->jack_tserver_cfg)
      || (!is_trans && !(future_prefs->jack_opts & JACK_OPTS_START_ASERVER)
          && !*future_prefs->jack_aserver_cfg)) {
    // could not connect and we were not allowed to start a server
    logmsg = lives_strdup_printf("#\n<b>%s client could not connect</b> to jack server '%s'%s\n"
                                 "\t\t\t\tThe client is not configured to start up a server of its own\n\n",
                                 type_name, (tmp = lives_markup_escape_text(server_name, -1)), (status & JackServerFailed) ?
                                 _(" (server not running)") : "");
    lives_free(tmp);
    widget_opts.use_markup = TRUE;
    jack_log_errmsg(jackd, logmsg);
    widget_opts.use_markup = FALSE;
    logmsg2 = lives_text_strip_markup(logmsg + 1);
    LIVES_ERROR(logmsg2);
    lives_free(logmsg); lives_free(logmsg2);
    goto ret_failed;
  }

  if (!con_attempts) logmsg2 = _(", proceding to next step.");
  else logmsg2 = lives_strdup("");

  // connect failed, try first with config file if we have one
  logmsg = lives_strdup_printf(_("#\n<b>%s client could not connect</b> to jack server named '%s'%s%s\n\n"),
                               type_name, (tmp = lives_markup_escape_text(server_name, -1)), (status & JackServerFailed) ?
                               _(" (server not running)") : "", logmsg2);
  lives_free(tmp);
  widget_opts.use_markup = TRUE;
  jack_log_errmsg(jackd, logmsg);
  widget_opts.use_markup = FALSE;
  lives_free(logmsg); lives_free(logmsg2);

  // try to connect audio
  lives_free(server_name);

  // check first, if we are asked to start a server and another client already started it or connected to it
  // just connect to that (e.g audio & transport clients have different connect servers, but the same startup server,
  // if both fail to connect, then audio should just connect to the server that transport started and
  // not try to start it again)
  if (is_trans) {
    if (*future_prefs->jack_tserver_sname) server_name = lives_strdup(future_prefs->jack_tserver_sname);
    else server_name = lives_strdup(defservname);
    if (as_running && !lives_strcmp(as_running, server_name)) {
      ts_running = server_name;
      ts_started = as_started;
      twins = TRUE;
      logmsg = lives_strdup_printf("#\n%s client '%s' was configured to start a server named '%s'; a jack v%d server with this name "
                                   "is already running, thus startup is unneccesary\n",
                                   type_name, client_name, server_name, jackver);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
      goto do_connect;
    }
    twins = FALSE;
  } else {
    if (*future_prefs->jack_aserver_cfg) server_name = lives_strdup(future_prefs->jack_aserver_cname);
    else {
      if (*future_prefs->jack_aserver_sname) server_name = lives_strdup(future_prefs->jack_aserver_sname);
      else server_name = lives_strdup(defservname);
      if (ts_running && !lives_strcmp(ts_running, server_name)) {
        as_running = server_name;
        as_started = ts_started;
        twins = TRUE;
        logmsg = lives_strdup_printf("#\n%s client '%s' was configured to start a server named '%s'; a jack v%d server with this name "
                                     "is already running, thus startup is unneccesary\n",
                                     type_name, client_name, server_name, jackver);
        jack_log_errmsg(jackd, logmsg);
        lives_free(logmsg);
        goto do_connect;
      }
      twins = FALSE;
    }
  }

  if (con_attempts++ > MAX_CONX_TRIES) {
    logmsg = lives_strdup_printf("#failed to connect to the server named '%s', perhaps there was an error in the config file "
                                 "or the given server name does not match the one in the script\n", server_name);
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
    goto ret_failed;
  }

  if (!lives_strcmp(future_prefs->jack_aserver_cname, future_prefs->jack_aserver_sname)) logmsg2 = _("start it instead");
  else logmsg2 = _("start a server of our own");

  logmsg = lives_strdup_printf(_("failed to connect to the server. Will now attempt to %s..."), logmsg2);
  jack_log_errmsg(jackd, logmsg);
  LIVES_ERROR(logmsg);
  lives_free(logmsg); lives_free(logmsg2);


  if (!script_tried) {
    // using a config file. We will run it via fork() and then try to connect
    // for this to work we need to parse the config file and try to extract the server name
    script_tried = TRUE;
    if (self) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
    if (is_trans) {
      scrfile = future_prefs->jack_tserver_cfg;
    } else {
      scrfile = future_prefs->jack_aserver_cfg;
    }
    //com = lives_fread_line(scrfile);
    com = lives_strdup_printf("cat '%s' | $SHELL &", scrfile);

    if (com) {
      int pidchk;
      if (is_trans) {
        ts_scripted = FALSE;
        if (*future_prefs->jack_tserver_cname) server_name = lives_strdup(future_prefs->jack_tserver_cname);
        else server_name = lives_strdup(defservname);
      } else {
        as_scripted = FALSE;
        if (*future_prefs->jack_aserver_cname) server_name = lives_strdup(future_prefs->jack_aserver_cname);
        else server_name = lives_strdup(defservname);
      }

      if (!sc_pid) {
        logmsg = lives_strdup_printf("LiVES: config file '%s' will be deployed", scrfile);
        jack_log_errmsg(jackd, logmsg);
        lives_free(logmsg);

        sc_pid = lives_fork(com);
        lives_free(com);
        if (!sc_pid) goto ret_failed;
        if (is_trans) tserver_pid = sc_pid;
        else aserver_pid = sc_pid;
      }

      pidchk = lives_kill(sc_pid, 0);
      if (!pidchk) {
        if (is_trans) ts_scripted = TRUE;
        else as_scripted = TRUE;
        logmsg = lives_strdup_printf("LiVES: config file running, retrying connection attempt to server named '%s'", server_name);
      } else {
        logmsg = lives_strdup_printf("LiVES: config file not running, will abandon attempt to connect to '%s'", server_name);
      }

      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);

      if (!pidchk) {
        if (self) lives_proc_thread_include_states(self, THRD_STATE_BUSY);
        if (con_attempts > 1) {
          lives_nanosleep(LIVES_WAIT_A_SEC);
          if (self) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
        }

        //if (self) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
        goto retry_connect;
      }
    } else {
      if (is_trans) ts_scripted = FALSE;
      else as_scripted = FALSE;
    }
  }

  // start a server using internal settings

  if (!jackserver) {
    // create the server object
    //jackserver = jackctl_server_create2(NULL, NULL, NULL);
    jackserver = jackctl_server_create(NULL, NULL);
    if (!jackserver) {
      logmsg = lives_strdup_printf("jackctl error: Could not create jackctl server object");
      jack_log_errmsg(jackd, logmsg);
      LIVES_ERROR(logmsg);
      lives_free(logmsg);
      lives_free(server_name);
      goto ret_failed;
    }
  }

  if (is_trans) prefs->jack_tserver = jackserver;
  else prefs->jack_aserver = jackserver;

#ifdef JACK_SYNC_MODE
  set_server_sync = TRUE;
#endif
  if (is_trans && !(future_prefs->jack_opts & JACK_OPTS_PERM_TSERVER))
    set_server_temp = TRUE;
  if (!is_trans && !(future_prefs->jack_opts & JACK_OPTS_PERM_ASERVER))
    set_server_temp = TRUE;

  if (is_test) set_server_temp = TRUE;

  params = jackctl_server_get_parameters(jackserver);

  if (is_trans) prefs->jack_tsparams = jack_params_to_rfx(params, prefs->jack_tserver);
  else prefs->jack_asparams = jack_params_to_rfx(params, prefs->jack_aserver);

  if (set_server_sync || set_server_temp) {
    // list server parameters
    while (params) {
      jackctl_parameter_t *parameter = (jackctl_parameter_t *)params->data;
      if (!strcmp(jackctl_parameter_get_name(parameter), "name")) {
        union jackctl_parameter_value value;
        snprintf(value.str, JACK_PARAM_STRING_MAX, "%s", server_name);
        jackctl_parameter_set_value(parameter, &value);
      }

      if (set_server_sync) {
        if (!strcmp(jackctl_parameter_get_name(parameter), "sync")) {
          union jackctl_parameter_value value;
          value.b = TRUE;
          jackctl_parameter_set_value(parameter, &value);
        }
      }
      if (set_server_temp) {
        if (!strcmp(jackctl_parameter_get_name(parameter), "temporary")) {
          union jackctl_parameter_value value;
          value.b = TRUE;
          jackctl_parameter_set_value(parameter, &value);
        }
      }
      params = jack_slist_next(params);
    }
  }

  drivers = jackctl_server_get_drivers_list(jackserver);

  if (is_trans) prefs->jack_tdrivers = drivers;
  else prefs->jack_adrivers = drivers;

  if ((is_trans && !future_prefs->jack_tdriver) || (!is_trans && !future_prefs->jack_adriver)) {
    // prompt user for driver
    boolean all_equal = FALSE;
    jackctl_driver_t *new_driver;
    LiVESList *slist = NULL;
    const char *asname, *tsname;
    // tell the thread monitor kindly not to terminate us for taking to long
    if (*future_prefs->jack_aserver_sname) asname = future_prefs->jack_aserver_sname;
    else asname = defservname;
    if (*future_prefs->jack_tserver_sname) tsname = future_prefs->jack_tserver_sname;
    else tsname = defservname;
    if (!lives_strcmp(asname, tsname)) all_equal = TRUE;
    if (is_trans && !all_equal && !prefs->jack_srv_dup) {
      uint64_t ostate = 0;
      if (self) ostate = lives_proc_thread_include_states(self, THRD_STATE_BUSY);
      main_thread_execute(get_def_drivers,
                          WEED_SEED_VOIDPTR, &new_driver, "vvb", drivers, &slist, 4);
      if (!new_driver) {
        if (mainw->cancelled) {
          logmsg = lives_strdup_printf("LiVES: User cancelled during driver selection for %s client", type_name);
          jack_log_errmsg(jackd, logmsg);
          lives_free(logmsg);
          goto ret_failed;
        }
        if (!self) goto ret_failed;
        if (!(ostate & THRD_STATE_BUSY)) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
        goto retry_connect;
      }
      future_prefs->jack_tdriver = lives_strdup(jackctl_driver_get_name(new_driver));
      if (future_prefs->jack_tslaves && future_prefs->jack_tslaves != future_prefs->jack_aslaves)
        lives_list_free(future_prefs->jack_tslaves);
      future_prefs->jack_tslaves = slist;
    } else {
      uint64_t ostate = 0;
      if (self) ostate = lives_proc_thread_include_states(self, THRD_STATE_BUSY);
      main_thread_execute(get_def_drivers,
                          WEED_SEED_VOIDPTR, &new_driver, "vvb", drivers, &slist, is_trans ? 4 : 5);
      if (!new_driver) {
        if (mainw->cancelled) {
          logmsg = lives_strdup_printf("LiVES: User cancelled during driver selection for %s client", type_name);
          jack_log_errmsg(jackd, logmsg);
          lives_free(logmsg);
          goto ret_failed;
        }
        if (!self) goto ret_failed;
        if (!(ostate & THRD_STATE_BUSY)) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
        goto retry_connect;
      }
      if (self && !(ostate & THRD_STATE_BUSY)) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);
      future_prefs->jack_adriver = lives_strdup_free(future_prefs->jack_adriver, jackctl_driver_get_name(new_driver));
      if (future_prefs->jack_aslaves && future_prefs->jack_aslaves != future_prefs->jack_tslaves)
        lives_list_free(future_prefs->jack_aslaves);
      future_prefs->jack_aslaves = slist;
    }
    if (all_equal) {
      future_prefs->jack_tdriver = lives_strdup_free(future_prefs->jack_tdriver, future_prefs->jack_adriver);
      future_prefs->jack_tslaves = future_prefs->jack_aslaves;
    }
  }

  if (is_trans) {
    driver_name = future_prefs->jack_tdriver;
    slave_list = future_prefs->jack_tslaves;
  } else {
    driver_name = future_prefs->jack_adriver;
    slave_list = future_prefs->jack_aslaves;
  }

  while (drivers) {
    driver = (jackctl_driver_t *)drivers->data;
    if (!lives_strcmp(driver_name, jackctl_driver_get_name(driver))) {
      logmsg = lives_strdup_printf("%s client has been configured to use '%s' driver",
                                   type_name, driver_name);
      jack_log_errmsg(jackd, logmsg);
      lives_free(logmsg);
      break;
    }
    drivers = jack_slist_next(drivers);
  }
  if (!drivers) {
    logmsg = lives_strdup_printf("#Could not find driver %s for jackd %s client in server named '%s'",
                                 driver_name, type_name, server_name);
    jack_log_errmsg(jackd, logmsg);
    logmsg2 = lives_text_strip_markup(logmsg + 1);
    LIVES_ERROR(logmsg2);
    lives_free(logmsg); lives_free(logmsg2);
    goto ret_failed;
  }

#ifdef JACK_V2
  if (is_trans) {
    mainw->crash_possible = 3;
  } else {
    mainw->crash_possible = 4;
  }
  defer_lpt = lives_hook_append(NULL, THREAD_EXIT_HOOK, 0, defer_sigint_cb,
                                LIVES_INT_TO_POINTER(mainw->crash_possible));

  if (!mainw->signals_deferred) {
    // try to handle crashes in jack_server_open()
    set_signal_handlers((lives_sigfunc_t)defer_sigint);
    needs_sigs = TRUE;
  }

  if (self) lives_proc_thread_exclude_states(self, THRD_STATE_BUSY);

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

  if (!jackctl_server_open(jackserver, driver)) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    lives_hook_remove(defer_lpt);

    if (needs_sigs) {
      set_signal_handlers((lives_sigfunc_t)catch_sigint);
      mainw->crash_possible = 0;
    }

    logmsg = lives_strdup_printf("Could not launch jack2 server named '%s' with %s driver",
                                 server_name, driver_name);
    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    jack_log_errmsg(jackd, NULL);
    goto ret_failed;
  }

  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  lives_hook_remove(defer_lpt);

  if (is_trans) {
    mainw->crash_possible = 5;
  } else {
    mainw->crash_possible = 6;
  }

  defer_lpt = lives_hook_append(NULL, THREAD_EXIT_HOOK, 0, defer_sigint_cb,
                                LIVES_INT_TO_POINTER(mainw->crash_possible));

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  if (!jackctl_server_start(jackserver)) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    lives_hook_remove(defer_lpt);

    if (needs_sigs) {
      set_signal_handlers((lives_sigfunc_t)catch_sigint);
      mainw->crash_possible = 0;
    }
    logmsg = lives_strdup_printf("Could not start jack2 server named '%s' for %s client",
                                 server_name, type_name);
    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    jack_log_errmsg(jackd, NULL);
    goto ret_failed;
  }

#else

  if (is_trans) {
    mainw->crash_possible = 5;
  } else {
    mainw->crash_possible = 6;
  }
  defer_lpt = lives_hook_append(NULL, THREAD_EXIT_HOOK, HOOK_CB_SINGLE_SHOT,
                                defer_sigint_cb, LIVES_INT_TO_POINTER(mainw->crash_possible));

  if (!mainw->signals_deferred) {
    // try to handle crashes in jack_client_open()
    set_signal_handlers((lives_sigfunc_t)defer_sigint);
    needs_sigs = TRUE;
  }

  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  if (!jackctl_server_start(jackserver, driver)) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    lives_hook_remove(defer_lpt);
    if (needs_sigs) {
      set_signal_handlers((lives_sigfunc_t)catch_sigint);
      mainw->crash_possible = 0;
    }

    logmsg = lives_strdup_printf("Could not start jack1 server named '%s' with driver %s",
                                 server_name, driver_name);

    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    jack_log_errmsg(jackd, NULL);
    goto ret_failed;
  }

#endif

  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  lives_hook_remove(defer_lpt);

  if (needs_sigs) {
    set_signal_handlers((lives_sigfunc_t)catch_sigint);
    mainw->crash_possible = 0;
  }

  if (is_trans) {
    ts_started = TRUE;
    ts_running = server_name;
  } else {
    as_started = TRUE;
    ts_running = server_name;
  }

  logmsg = lives_strdup_printf(_("jack server named '%s' started for %s client using driver %s"),
                               server_name, type_name, jackctl_driver_get_name(driver));

  jack_log_errmsg(jackd, logmsg);
  lives_free(logmsg);
  jack_log_errmsg(jackd, NULL);

  if ((is_trans && (future_prefs->jack_opts & JACK_OPTS_SETENV_TSERVER)) ||
      (!is_trans && (future_prefs->jack_opts & JACK_OPTS_SETENV_ASERVER))) {
    lives_setenv(JACK_DEFAULT_SERVER, server_name);
    lives_snprintf(prefs->jack_def_server_name, JACK_PARAM_STRING_MAX,
                   "%s", server_name);
    logmsg = lives_strdup_printf(_("LiVES : exported %s as $%s"), server_name, JACK_DEFAULT_SERVER);
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
  }

  // startup the client now
do_connect:

  jackd->client = jack_client_open(client_name, options, &status, server_name);
  if (!jackd->client) {
    jack_log_errmsg(jackd, NULL);
    goto ret_failed;
  }

  if (!was_started) {
    logmsg = lives_strdup_printf(_("jack %s client connected to server named '%s'"), type_name, server_name);
    jack_log_errmsg(jackd, logmsg);
    d_print(logmsg);
    lives_free(logmsg);
  }

  if (was_started) goto ret_success;

connect_done:
  if (!is_trans && ts_started && twins) {
    // if ts_started and driver is wrong then we can change it
    // jackctl_server_switch_master(server, driver);
  }

  if (is_test) goto ret_success;

  // add slaves
  if (jackserver) {
    if (is_trans) slave_list = future_prefs->jack_tslaves;
    else slave_list = future_prefs->jack_aslaves;
    while (slave_list) {
      if (!jackctl_server_add_slave(jackserver, (jackctl_driver_t *)slave_list->data)) {
        logmsg = lives_strdup_printf(_("jackctl : failed adding slave %s to server named '%s'"),
                                     jackctl_driver_get_name((jackctl_driver_t *)slave_list->data),
                                     server_name);
        jack_log_errmsg(jackd, logmsg);
        LIVES_ERROR(logmsg);
        lives_free(logmsg);
        jack_log_errmsg(jackd, NULL);
      }
      slave_list = slave_list->next;
    }
  }

  if (!is_trans) goto ret_success;

  if (future_prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT) {
    jack_activate(jackd->client);
    jackd->sample_in_rate = jackd->sample_out_rate = jack_get_sample_rate(jackd->client);
    jack_set_sync_callback(jackd->client, start_ready_callback, jackd);
    jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);
    jack_on_shutdown(jackd->client, jack_shutdown, jackd);
    mainw->jack_trans_poll = TRUE;
  } else {
    jack_client_close(jackd->client);
    jackd->client = NULL;
  }

ret_success:
  if (!is_test) return TRUE;
  test_ret = TRUE;

ret_failed:
  if (!is_test) return FALSE;

  if (is_trans) finish_test(jackd, test_ret, TRUE, FALSE);

  if (tstwin) {
    if (tstwin->button) {
      lives_widget_grab_focus(tstwin->button);
    }
  }
  textwindow = tstwin;
  return test_ret;
}


/////////////////////////////////////////////////////////////////
// transport handling


ticks_t jack_transport_get_current_ticks(jack_driver_t *jackd) {
#ifdef ENABLE_JACK_TRANSPORT
  double val;
  jack_position_t pos;

  jack_transport_query(jackd->client, &pos);
  val = (double)pos.frame / (double)jackd->sample_out_rate;

  if (val > 0.) return val * TICKS_PER_SECOND_DBL;
#endif
  return -1;
}


#ifdef ENABLE_JACK_TRANSPORT
static void jack_transport_check_state(jack_driver_t *jackd) {
  jack_position_t pos;
  jack_transport_state_t jacktstate;

  // go away until the app has started up properly
  if (mainw->go_away) return;

  if (!jackd || !jackd->client) return;

  if (!(prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE)
      || !(prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)) return;

  jacktstate = jack_transport_query(jackd->client, &pos);

  if (jacktstate == JackTransportStopped) {
    if (mainw->jack_can_stop) {
      if (LIVES_IS_PLAYING) on_stop_activate(NULL, NULL);
      mainw->jack_can_stop = FALSE;
    }
    if (!LIVES_IS_PLAYING) mainw->jack_can_start = TRUE;
    return;
  }

  if (!mainw->jack_can_start && !mainw->jack_can_stop) return;

  if (lives_get_status() == LIVES_STATUS_IDLE
      && mainw->jack_can_start && (jacktstate == JackTransportRolling || jacktstate == JackTransportStarting)
      && CURRENT_CLIP_IS_VALID) {
    // stops us from immediately restarting after pressing Stop in LiVES
    mainw->jack_can_start = FALSE;
    mainw->jack_can_stop = TRUE;
    start_playback_async(0);
    return;
  }
}


static void jack_transport_make_master(jack_driver_t *jackd, boolean set) {
  if (!jackd || !jackd->client) return;
  if (set) {
    //if (!jack_set_timebase_callback(jackd->client, 1, timebase_callback, jackd))
    if (!jack_set_timebase_callback(jackd->client, 0, timebase_callback, jackd))
      mainw->jack_master = TRUE;
    else
      mainw->jack_master = FALSE;
    return;
  }
  jack_release_timebase(jackd->client);
  mainw->jack_master = FALSE;
}

#endif


void jack_transport_make_strict_slave(jack_driver_t *jackd, boolean set) {
#ifdef ENABLE_JACK_TRANSPORT
  if (!mainw || !mainw->is_ready || !jackd || !jackd->client) return;
  lives_widget_set_sensitive(mainw->playall, !set);
  lives_widget_set_sensitive(mainw->playsel, !set);
  lives_widget_set_sensitive(mainw->stop, !set);
  lives_widget_set_sensitive(mainw->rewind, !set);
  lives_widget_set_sensitive(mainw->spinbutton_pb_fps, !set);
  if (mainw->p_rewindbutton) lives_widget_set_sensitive(mainw->p_rewindbutton, !set);
  if (mainw->p_playbutton) lives_widget_set_sensitive(mainw->p_playbutton, !set);
  if (mainw->p_playselbutton) lives_widget_set_sensitive(mainw->p_playselbutton, !set);
  lives_widget_set_sensitive(mainw->m_playbutton, !set);
  lives_widget_set_sensitive(mainw->m_playselbutton, !set);
  lives_widget_set_sensitive(mainw->m_stopbutton, !set);
  lives_widget_set_sensitive(mainw->m_rewindbutton, !set);
#endif
}


boolean is_transport_locked(void) {
#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)
      && (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE))
    return TRUE;
#endif
  return FALSE;
}


boolean lives_jack_poll(void) {
  // must return TRUE
#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jackd_trans && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      && (prefs->jack_opts & JACK_OPTS_TRANSPORT_SLAVE))
    jack_transport_check_state(mainw->jackd_trans);
#endif
  return TRUE;
}


void lives_jack_end(void) {
  if (mainw->jackd_trans) {
    jack_client_t *client = mainw->jackd_trans->client;
    mainw->jackd_trans->client = NULL; // stop polling transport
    if (client) {
      jack_deactivate(client);
      jack_client_close(client);
    }
  }
  if (jackserver) jackctl_server_destroy(jackserver);
  jackserver = NULL;
}


void jack_transport_update(jack_driver_t *jackd, double pbtime) {
  if (jackd && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      && (prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART)) {
    jack_transport_locate(jackd->client, pbtime * jackd->sample_out_rate);
  }
}


void jack_pb_start(jack_driver_t *jackd, double pbtime) {
  if (mainw->jack_can_stop) return;
  if (jackd && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)) {
    mainw->lives_can_stop = TRUE;
    if (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER) {
      if (pbtime >= 0. && (prefs->jack_opts & JACK_OPTS_TIMEBASE_LSTART))
        jack_transport_update(jackd, pbtime);
    }
    if (prefs->jack_opts & JACK_OPTS_TIMEBASE_MASTER)
      jack_transport_make_master(jackd, TRUE);
    jack_transport_start(jackd->client);
  }
}


void jack_pb_stop(jack_driver_t *jackd) {
  // call this after pb stops
  if (mainw->lives_can_stop) {
    if (jackd && (prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
        && (prefs->jack_opts & JACK_OPTS_TRANSPORT_MASTER))
      jack_transport_stop(jackd->client);
    if (mainw->jack_master) jack_transport_make_master(jackd, FALSE);
    mainw->lives_can_stop = FALSE;
  }
}

////////////////////////////////////////////
// audio

void jack_set_avel(jack_driver_t *jackd, int clipno, double ratio) {
  if (jackd->playing_file == mainw->ascrap_file) return;
  if (!CLIP_HAS_AUDIO(clipno)) {
    jackd->sample_in_rate = jackd->sample_out_rate * ratio;
  } else {
    lives_clip_t *sfile = mainw->files[clipno];
    jackd->sample_in_rate = sfile->arps * ratio;
    sfile->adirection = LIVES_DIRECTION_SIG(ratio);
  }
  if (AV_CLIPS_EQUAL) jack_get_rec_avals(jackd);
}


// actually we only have 1 reader and 1 writer; trans client is callocated
static jack_driver_t outdev[JACK_MAX_OUTDEVICES];
static jack_driver_t indev[JACK_MAX_INDEVICES];

void jack_get_rec_avals(jack_driver_t *jackd) {
  if (RECORD_PAUSED || !LIVES_IS_RECORDING) return;
  if ((prefs->audio_opts & AUDIO_OPTS_IS_LOCKED) && mainw->ascrap_file != -1) {
    mainw->rec_aclip = mainw->ascrap_file;
    mainw->rec_aseek = mainw->files[mainw->ascrap_file]->aseek_pos;
    mainw->rec_avel = 1.;
    return;
  }
  mainw->rec_aclip = jackd->playing_file;
  if (CLIP_HAS_AUDIO(mainw->rec_aclip)) {
    if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_WRITER) {
      mainw->rec_aseek = (double)fwd_seek_pos / (double)(afile->achans * afile->asampsize / 8) / (double)afile->arps;
      mainw->rec_avel = fabs((double)jackd->sample_in_rate / (double)afile->arps) * (double)afile->adirection;
    } else {
      mainw->rec_aseek = (double)(afile->aseek_pos / (double)(afile->achans * afile->asampsize / 8)) / (double)afile->arps;
      mainw->rec_avel = 1.;
    }
  } else mainw->rec_avel = 0.;
}


static void jack_set_rec_avals(jack_driver_t *jackd) {
  // record direction change (internal)
  mainw->rec_aclip = jackd->playing_file;
  if (mainw->rec_aclip != -1) {
    jack_get_rec_avals(jackd);
  }
}


size_t jack_get_buffsize(jack_driver_t *jackd) {
  if (cache_buffer) return cache_buffer->bytesize;
  return 0;
}


static void push_cache_buffer(lives_audio_buf_t *cache_buffer, jack_driver_t *jackd,
                              size_t in_bytes, size_t nframes, double shrink_factor) {
  // push a cache_buffer for another thread to fill
  int qnt;
  if (!cache_buffer) return;

  pthread_mutex_lock(&cache_buffer->atomic_mutex);

  qnt = afile->achans * (afile->asampsize >> 3);
  jackd->seek_pos = align_ceilng(jackd->seek_pos, qnt);

  if (mainw->ascrap_file > -1 && jackd->playing_file == mainw->ascrap_file)
    cache_buffer->sequential = TRUE;
  else cache_buffer->sequential = FALSE;

  cache_buffer->fileno = jackd->playing_file;

  cache_buffer->seek = jackd->seek_pos;
  cache_buffer->bytesize = in_bytes;

  cache_buffer->in_achans = jackd->num_input_channels;
  cache_buffer->out_achans = jackd->num_output_channels;

  cache_buffer->in_asamps = afile->asampsize;
  cache_buffer->out_asamps = -32;  ///< 32 bit float

  cache_buffer->shrink_factor = shrink_factor;

  cache_buffer->swap_sign = jackd->usigned;
  cache_buffer->swap_endian = jackd->reverse_endian ? SWAP_X_TO_L : 0;

  cache_buffer->samp_space = nframes;

  cache_buffer->in_interleaf = TRUE;
  cache_buffer->out_interleaf = FALSE;

  cache_buffer->operation = LIVES_READ_OPERATION;
  pthread_mutex_unlock(&cache_buffer->atomic_mutex);

  wake_audio_thread();
}


LIVES_INLINE lives_audio_buf_t *pop_cache_buffer(void) {
  // get next available cache_buffer
  return audio_cache_get_buffer();
}


static void output_silence(size_t offset, jack_nframes_t nframes, jack_driver_t *jackd, float **out_buffer) {
  // write nframes silence to all output streams
  int nch = jackd->num_output_channels;
  for (int i = 0; i < nch; i++) {
    if (out_buffer[i]) {
      if (!jackd->is_silent) {
        sample_silence_dS(out_buffer[i] + offset, nframes);
      }
      if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT) {
        // audio to be sent to video generator plugins
        append_to_audio_bufferf(out_buffer[i] + offset, nframes, i == nch - 1 ? -i - 1 : i + 1);
      }
    }
  }
  if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
    // audio to be sent to video playback plugin
    sample_silence_stream(nch, nframes);
  }
  if (jackd->astream_fd != -1) {
    // external streaming
    size_t rbytes = nframes * nch * 2;
    check_zero_buff(rbytes);
    audio_stream(zero_buff, rbytes, jackd->astream_fd);
  }
  if (mainw->video_seek_ready && mainw->audio_seek_ready
      && !jackd->is_paused) jackd->frames_written += nframes;
}


static void mixdown_aux(jack_driver_t *jackd, float **buff, float * ratios, jack_nframes_t nframes) {
  int i, nch = jackd->num_output_channels;
  // ratio of 0. means no aux
  for (i = 0; i < nch; i++) if (ratios[i] > 0.) break;

  if (i < nch) {
    static float **lb_buff = NULL;
    static size_t lbufsiz = 0;
    float **ac_buff = NULL;
    float *aux_buff[JACK_MAX_PORTS];
    size_t acsize, remsiz, xlbufsiz;
    // get audio from abuffs
    // create a fake audio chan
    weed_plant_t *achan = weed_plant_new(WEED_PLANT_CHANNEL);

    weed_channel_set_audio_data(achan, NULL, jackd->sample_out_rate, nch, 0);
    fill_audio_channel_aux(achan);
    acsize = weed_channel_get_audio_length(achan);

    // audio_data_length tells if we have more audio
    // we just append this to loopback_buff

    // ideally we want just enough in lb buff to fill nframes
    // if it is over thresh, drop some from start and reduce thresh
    // if too small, increase thresh

    // after adjusting, copy up to nframes from lb buff, if not enough, fill from achan
    // append remainder of achan to lb buff

    if (!lb_buff) {
      lb_buff = (float **)lives_calloc(nch, sizeof(float *));
      for (i = 0; i < nch; i++) lb_buff[i] = NULL;
    }

    xlbufsiz = lbufsiz;
    if (xlbufsiz > nframes) xlbufsiz = nframes;

    remsiz = nframes - xlbufsiz;

    if (acsize) {
      ac_buff = weed_channel_get_audio_data(achan, NULL);
    }

    // underflow
    if (remsiz > acsize) remsiz = acsize;

    for (i = 0; i < nch; i++) {
      aux_buff[i] = (float *)jack_port_get_buffer(jackd->output_port[i + nch], nframes);
      if (xlbufsiz) lives_memcpy(aux_buff[i], lb_buff[i], xlbufsiz * 4);
      if (remsiz) {
        lives_memcpy((void *)aux_buff[i] + xlbufsiz * 4, ac_buff[i], remsiz * 4);
      }
      //
      if (lbufsiz > xlbufsiz || acsize > remsiz) {
        if (acsize > xlbufsiz) {
          float *tmp = (float *)lives_calloc(lbufsiz - xlbufsiz + acsize - remsiz, 4);
          if (xlbufsiz < lbufsiz) lives_memcpy((void *)tmp, (void *)lb_buff[i] + xlbufsiz * 4, (lbufsiz - xlbufsiz) * 4);
          if (acsize > remsiz)
            lives_memcpy((void *)tmp + (lbufsiz - xlbufsiz) * 4, (void *)ac_buff[i] + remsiz * 4, (acsize - remsiz) * 4);
          if (lb_buff[i]) lives_free(lb_buff[i]);
          lb_buff[i] = tmp;
        } else {
          if (xlbufsiz < lbufsiz)
            lives_memmove((void *)lb_buff[i], (void *)lb_buff[i] + xlbufsiz * 4, (lbufsiz - xlbufsiz) * 4);
          if (remsiz < acsize)
            lives_memcpy((void *)lb_buff[i] + (lbufsiz - xlbufsiz) * 4, (void *)ac_buff[i] + remsiz * 4, (acsize - remsiz) * 4);
        }
      }
    }

    weed_plant_free(achan);
    if (ac_buff) lives_free(ac_buff);
    lbufsiz = lbufsiz - xlbufsiz + acsize - remsiz;

    // now we simply need to mix aux_buff with buff in the defined ratio
    for (i = 0; i < nch; i++) {
      float ratio = ratios[i], iratio = 1. - ratio;
      for (int j = 0; j < nframes; j++) {
        buff[i][j] = buff[i][j] * iratio + aux_buff[i][j] * ratio;
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void lives_jack_set_client_attributes(jack_driver_t *jackd, int fileno, boolean activate, boolean running) {
  if (IS_VALID_CLIP(fileno)) {
    lives_clip_t *sfile = mainw->files[fileno];
    int asigned = !(sfile->signed_endian & AFORM_UNSIGNED);
    int aendian = !(sfile->signed_endian & AFORM_BIG_ENDIAN);

    // called from CMD_FILE_OPEN and also prepare...
    if (!running || (jackd && mainw->aud_rec_fd == -1)) {
      if (!running) jackd->is_paused = FALSE;
      else {
        jackd->is_paused = afile->play_paused;
        jackd->mute = mainw->mute;
      }

      if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
        if (mainw->ping_pong && prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS && !mainw->multitrack
            && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
          jackd->loop = AUDIO_LOOP_PINGPONG;
        else jackd->loop = AUDIO_LOOP_FORWARD;
      } else jackd->loop = AUDIO_LOOP_NONE;

      if ((activate || running) && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)) {
        if (!sfile->play_paused)
          jackd->sample_in_rate = sfile->arate * sfile->pb_fps / sfile->fps;
        else jackd->sample_in_rate = sfile->arate * sfile->freeze_fps / sfile->fps;
      } else jackd->sample_in_rate = sfile->arate;
      if (sfile->adirection == LIVES_DIRECTION_REVERSE)
        jackd->sample_in_rate = -abs(jackd->sample_in_rate);
      else
        jackd->sample_in_rate = abs(jackd->sample_in_rate);

      jackd->usigned = !asigned;
      jackd->seek_end = sfile->afilesize;

      jackd->num_input_channels = sfile->achans;
      jackd->bytes_per_channel = sfile->asampsize / 8;

      if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
          || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
        jackd->reverse_endian = TRUE;
      else jackd->reverse_endian = FALSE;
    }
  }
}


static volatile boolean in_ap = FALSE;

static int audio_process(jack_nframes_t nframes, void *arg) {
  // JACK calls this periodically to get the next audio buffer
  GET_PROC_THREAD_SELF(self);
  float *out_buffer[JACK_MAX_PORTS];
  jack_driver_t *jackd = (jack_driver_t *)arg;
  jack_position_t pos;
  aserver_message_t *msg;
  int64_t xseek;
  int new_file;
  int nch;
  static boolean reset_buffers = FALSE;
  boolean from_memory = FALSE;
  boolean wait_cache_buffer = FALSE;
  boolean pl_error = FALSE; ///< flag tells if we had an error during plugin processing
  size_t nbytes, rbytes;

  int i;

  in_ap = TRUE;

  static lives_thread_data_t *tdata = NULL;

  if (!tdata) {
    tdata = get_thread_data();
    // pulsed->inst will be our aplayer object instance
    // as a stopgap, we can treat the aplayer instance as a lives_proc_thread
    // in the sense that it runs this function, but by being "queued" by an external entity
    self = jackd->inst;
    lives_thread_switch_self(self, FALSE);
    lives_snprintf(tdata->vars.var_origin, 128, "%s", "pulseaudio writer Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
  }

  lives_proc_thread_include_states(self, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(self, THRD_STATE_IDLING);

  nch = jackd->num_output_channels;

  //#define DEBUG_AJACK

  if (!mainw->is_ready || !jackd || (!LIVES_IS_PLAYING && jackd->is_silent && !jackd->msgq)) {
    in_ap = FALSE;
    reset_buffers = TRUE;
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return 0;
  }

  //jackd->is_silent = FALSE;

  /* retrieve the buffers for the output ports */
  for (i = 0; i < nch; i++)
    out_buffer[i] = (float *)jack_port_get_buffer(jackd->output_port[i], nframes);

  if (AUD_SRC_EXTERNAL && (prefs->audio_opts & AUDIO_OPTS_EXT_FX)) {
    // "loopback" buffers - generally we read exactly as much as we write, but anything left over gets cached
    static float **lb_buff = NULL;
    static size_t lbufsiz = 0;

    float **ac_buff = NULL;
    float vol;
    size_t acsize, remsiz, xlbufsiz;
    int k;

    // get audio from abuffs
    // create a fake audio chan
    weed_plant_t *achan = weed_plant_new(WEED_PLANT_CHANNEL);

    if (reset_buffers && lb_buff) {
      for (i = 0; i < nch; i++) if (lb_buff[i]) lives_free(lb_buff[i]);
      lives_free(lb_buff);
      lb_buff = NULL;
      lbufsiz = 0;
    }

    if (!jackd->in_use && !jackd->msgq) {
      if (!jackd->is_silent) {
        output_silence(0, nframes, jackd, out_buffer);
        jackd->is_silent = TRUE;
      }
      lives_proc_thread_include_states(self, THRD_STATE_IDLING);
      lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
      in_ap = FALSE;
      return 0;
    }

    reset_buffers = FALSE;

    if (CLIP_HAS_AUDIO(jackd->playing_file) && !mainw->multitrack)
      vol = lives_vol_from_linear(future_prefs->volume * afile->vol);
    else vol = lives_vol_from_linear(future_prefs->volume);

    weed_channel_set_audio_data(achan, NULL, jackd->sample_out_rate, nch, 0);
    fill_audio_channel(NULL, achan, FALSE);

    acsize = weed_channel_get_audio_length(achan);

    // audio_data_length tells if we have more audio
    // we just append this to loopback_buff

    // ideally we want just enough in lb buff to fill nframes
    // if it is over thresh, drop some from start and reduce thresh
    // if too small, increase thresh

    // after adjusting, copy up to nframes from lb buff, if not enough, fill from achan
    // append remainder of achan to lb buff

    if (!lb_buff) {
      lb_buff = (float **)lives_calloc(nch, sizeof(float *));
    }

    xlbufsiz = lbufsiz;
    if (xlbufsiz > nframes) xlbufsiz = nframes;
    remsiz = nframes - xlbufsiz;

    if (acsize) {
      ac_buff = weed_channel_get_audio_data(achan, NULL);
      if (!ac_buff) acsize = 0;
    }

    // underflow
    if (remsiz > acsize) remsiz = acsize;

    for (i = 0; i < nch; i++) {
      if (out_buffer[i]) {
        if (xlbufsiz) {
          for (k = 0; k < xlbufsiz; k++)
            out_buffer[i][k] = lb_buff[i][k] * vol;
        }
        if (remsiz) {
          if (ac_buff[i]) {
            for (k = 0; k < remsiz; k++)
              out_buffer[i][xlbufsiz + k] = ac_buff[i][k] * vol;
          }
        }
      }
      //
      if (lbufsiz > xlbufsiz || acsize > remsiz) {
        if (acsize > xlbufsiz) {
          float *tmp = (float *)lives_calloc(lbufsiz - xlbufsiz + acsize - remsiz, 4);
          if (xlbufsiz < lbufsiz) lives_memcpy((void *)tmp, (void *)lb_buff[i] + xlbufsiz * 4, (lbufsiz - xlbufsiz) * 4);
          if (acsize > remsiz)
            lives_memcpy((void *)tmp + (lbufsiz - xlbufsiz) * 4, (void *)ac_buff[i] + remsiz * 4, (acsize - remsiz) * 4);
          if (lb_buff[i]) lives_free(lb_buff[i]);
          lb_buff[i] = tmp;
        } else {
          if (xlbufsiz < lbufsiz && xlbufsiz)
            lives_memmove((void *)lb_buff[i], (void *)lb_buff[i] + xlbufsiz * 4, (lbufsiz - xlbufsiz) * 4);

          if (remsiz < acsize && ac_buff && ac_buff[i]) {
            lives_memcpy((void *)lb_buff[i] + (lbufsiz - xlbufsiz) * 4, (void *)ac_buff[i] + remsiz * 4, (acsize - remsiz) * 4);
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*

    weed_plant_free(achan);
    if (ac_buff) lives_free(ac_buff);
    lbufsiz = lbufsiz - xlbufsiz + acsize - remsiz;

    if (has_audio_filters(AF_TYPE_NONA)) {
      float **xfltbuf;
      ticks_t tc = mainw->currticks;
      // apply inplace any effects with audio in_channels
      weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
      weed_layer_set_audio_data(layer, out_buffer, jackd->sample_out_rate, nch, nframes);
      weed_set_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
      weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
      xfltbuf = weed_layer_get_audio_data(layer, NULL);
      for (i = 0; i < nch; i++) {
        if (xfltbuf[i] != out_buffer[i]) {
          lives_memcpy(out_buffer[i], xfltbuf[i], nframes * 4);
          lives_free(xfltbuf[i]);
        }
      }
      lives_free(xfltbuf);
      weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
      weed_layer_unref(layer);
    }
    if (prefs->audio_opts & AUDIO_OPTS_AUX_PLAY) {
      float auxmix[2] = {0.5, 0.5};
      mixdown_aux(jackd, out_buffer, auxmix, nframes);
    }
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    in_ap = FALSE;
    return 0;
  }

  if ((!jackd->in_use || !nframes) && !mainw->xrun_active && !jackd->msgq) {
    if (!jackd->is_silent) {
      output_silence(0, nframes, jackd, out_buffer);
      jackd->is_silent = TRUE;
    }
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    in_ap = FALSE;
    return 0;
  }

  reset_buffers = FALSE;

  /* process one message */
  if ((msg = (aserver_message_t *)jackd->msgq) != NULL) {
    uint64_t in_bytes;
    int64_t in_frames = 0;
    switch (msg->command) {
    case ASERVER_CMD_FILE_OPEN:
      new_file = atoi((char *)msg->data);
      if (jackd->playing_file != new_file) {
        jackd->playing_file = new_file;
        lives_jack_set_client_attributes(jackd, new_file, FALSE, TRUE);
      }
      fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = 0;
      push_cache_buffer(cache_buffer, jackd, 0, 0, 1.);
      break;
    case ASERVER_CMD_FILE_CLOSE:
      jackd->playing_file = -1;
      jackd->in_use = FALSE;
      jackd->seek_pos = jackd->real_seek_pos = fwd_seek_pos = 0;
      break;
    case ASERVER_CMD_FILE_SEEK:
    case ASERVER_CMD_FILE_SEEK_ADJUST:
      if (jackd->playing_file < 0) break;
      xseek = atol((char *)msg->data);
      if (msg->command == ASERVER_CMD_FILE_SEEK_ADJUST) {
        ticks_t delta = lives_get_current_ticks() - msg->tc;
        xseek += (double)delta / TICKS_PER_SECOND_DBL  *
                 (double)(afile->adirection * afile->arate * afile->achans * (afile->asampsize >> 3));
      }

      xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));
      if (xseek < 0) xseek = 0;

      fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = afile->aseek_pos = xseek;
      if (msg->extra) {
        double ratio = lives_strtod(msg->extra);
        jack_set_avel(jackd, jackd->playing_file, ratio);
      }
      jackd->in_use = TRUE;
      in_bytes = ABS((in_frames = ((double)jackd->sample_in_rate / (double)jackd->sample_out_rate *
                                   (double)nframes + ((double)fastrand() / (double)LIVES_MAXUINT64))))
                 * jackd->num_input_channels * jackd->bytes_per_channel;
      push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, 1.0);
      break;
    default:
      jackd->msgq = NULL;
      msg->data = NULL;
    }

    if (msg->next != msg) lives_freep((void **) & (msg->data));
    msg->command = ASERVER_CMD_PROCESSED;
    jackd->msgq = msg->next;
    if (jackd->msgq && jackd->msgq->next == jackd->msgq) jackd->msgq->next = NULL;

    if (!jackd->is_silent) {
      output_silence(0, nframes, jackd, out_buffer);
      jackd->is_silent = TRUE;
    }
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    in_ap = FALSE;
    return 0;
  }

  if (mainw->video_seek_ready) {
    if (!mainw->audio_seek_ready) {
      audio_sync_ready();
    }
  } else {
    if (!jackd->is_silent) {
      output_silence(0, nframes, jackd, out_buffer);
      jackd->is_silent = TRUE;
    }
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    in_ap = FALSE;
    return 0;
  }

  if ((mainw->agen_key == 0 || mainw->agen_needs_reinit || mainw->multitrack || mainw->preview)
      && jackd->in_use && jackd->playing_file > -1) {
    //if ((mainw->agen_key == 0 || mainw->agen_needs_reinit || mainw->multitrack) && jackd->in_use) {
    // if a plugin is generating audio we do not use cache_buffers, otherwise:
    if (jackd->read_abuf == -1) {
      // assign local copy from cache_buffers
      if (!LIVES_IS_PLAYING || (cache_buffer = pop_cache_buffer()) == NULL) {
        // audio buffer is not ready yet

        if (!jackd->is_silent) {
          output_silence(0, nframes, jackd, out_buffer);
          jackd->is_silent = TRUE;
        }
        lives_proc_thread_include_states(self, THRD_STATE_IDLING);
        lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
        in_ap = FALSE;
        return 0;
      }
      if (cache_buffer->fileno == -1) jackd->playing_file = -1;
    }
    if (cache_buffer && cache_buffer->in_achans > 0 && !cache_buffer->is_ready) {
      wait_cache_buffer = TRUE;
      if (!jackd->is_silent) {
        output_silence(0, nframes, jackd, out_buffer);
        jackd->is_silent = TRUE;
        //if (!mainw->audio_seek_ready && !jackd->is_paused) jackd->frames_written -= nframes;
        /* } else { */
        /* 	if (mainw->audio_seek_ready && !jackd->is_paused) jackd->frames_written += nframes; */
        /* 	if (mainw->audio_seek_ready) { */
        /* 	  if (jackd->playing_file >= 0) { */
        /* 	    int64_t xseek = jackd->seek_pos + ABS(((double)jackd->sample_in_rate / (double)jackd->sample_out_rate * */
        /* 						   (double)nframes + ((double)fastrand() */
        /* 								      / (double)LIVES_MAXUINT64))) */
        /* 	      * jackd->num_input_channels * jackd->bytes_per_channel; */
        /* 	    xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3)); */
        /*     fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = afile->aseek_pos = xseek; */
        /*   } */
      }
      if (mainw->audio_seek_ready) {
        //mainw->xrun_active = TRUE;
        lives_proc_thread_include_states(self, THRD_STATE_IDLING);
        lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
        in_ap = FALSE;
        return 0;
      }
    }
  }

  mainw->xrun_active = FALSE;

  fwd_seek_pos = jackd->real_seek_pos = jackd->seek_pos;
  jackd->state = jack_transport_query(jackd->client, &pos);

#ifdef DEBUG_AJACK
  lives_printerr("STATE is %d\n", jackd->state);
#endif

  //g_print("MAX DEL is %f\n", jack_get_max_delayed_usecs(jackd->client));

  /* handle playing state */
  if (!(prefs->jack_opts & JACK_OPTS_STRICT_SLAVE)
      || !(prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)
      || jackd->state == JackTransportRolling) {
    uint64_t jackFramesAvailable = nframes; /* frames we have left to write to jack */
    uint64_t inputFramesAvailable;          /* frames we have available this loop */
    uint64_t numFramesToWrite;              /* num frames we are writing this loop */
    //int64_t in_frames = 0;
    uint64_t in_bytes = 0, xin_bytes = 0;
    float shrink_factor = 1.f;
    double vol = 1.;
    double in_framesd = 0.;
    lives_clip_t *xfile = afile;
    int qnt = 1;
    if (IS_VALID_CLIP(jackd->playing_file)) qnt = afile->achans * (afile->asampsize >> 3);

#ifdef DEBUG_AJACK
    lives_printerr("playing... jackFramesAvailable = %ld %ld\n", jackFramesAvailable, fwd_seek_pos);
#endif

    jackd->num_calls++;

    if (!jackd->in_use || !mainw->video_seek_ready
        || ((jackd->playing_file < 0 || jackd->seek_pos < 0.) && jackd->read_abuf < 0
            && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit)
                || mainw->multitrack || mainw->preview))
        || jackd->is_paused) {
      /* output silence if nothing is being outputted */
      if (!jackd->is_silent) {
        output_silence(0, nframes, jackd, out_buffer);
        jackd->is_silent = TRUE;
      }
      lives_proc_thread_include_states(self, THRD_STATE_IDLING);
      lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
      in_ap = FALSE;
      return 0;
    }

    jackd->is_silent = FALSE;

    if (!mainw->xrun_active) jackd->last_proc_ticks = lives_get_current_ticks();

    /* g_print("VALXX %ld %d %d %d %d\n", jackFramesAvailable, jackd->read_abuf, mainw->agen_key, */
    /*         mainw->agen_needs_reinit, mainw->preview); */
    if (LIVES_LIKELY(jackFramesAvailable > 0 || (jackd->read_abuf > -1
                     || (((mainw->agen_key != 0 || mainw->agen_needs_reinit)
                          && !mainw->preview) && !mainw->multitrack)))) {
      if (LIVES_IS_PLAYING && jackd->read_abuf > -1) {
        // playing back from memory buffers instead of from file
        // this is used in multitrack
        from_memory = TRUE;

        numFramesToWrite = jackFramesAvailable;
        jackd->frames_written += numFramesToWrite;
        //jackFramesAvailable = 0;
        mainw->xrun_active = FALSE;
      } else {
        boolean eof = FALSE;
        int playfile = mainw->playing_file;
        if (mainw->xrun_active) {
          if (!jackd->is_silent) {
            output_silence(0, nframes, jackd, out_buffer);
            jackd->is_silent = TRUE;
          } else {
            int64_t xseek = jackd->seek_pos + nframes * afile->achans * (afile->asampsize >> 3);
            xseek = ALIGN_CEIL64(xseek, afile->achans * (afile->asampsize >> 3));
            fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = afile->aseek_pos = xseek;
            jackd->frames_written += nframes;
          }
          mainw->xrun_active = FALSE;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          in_ap = FALSE;
          return 0;
        }

        jackd->seek_end = 0;
        if (mainw->agen_key == 0 && !mainw->agen_needs_reinit && IS_VALID_CLIP(jackd->playing_file)) {
          if (mainw->playing_sel) {
            jackd->seek_end = (int64_t)((double)(afile->end - 1.) / afile->fps * afile->arps) * afile->achans
                              * (afile->asampsize / 8);
            if (jackd->seek_end > afile->afilesize) jackd->seek_end = afile->afilesize;
          } else {
            if (!mainw->loop_video) jackd->seek_end = (int64_t)((double)(mainw->play_end - 1.) / afile->fps * afile->arps)
                  * afile->achans * (afile->asampsize / 8);
            else jackd->seek_end = afile->afilesize;
          }
          if (jackd->seek_end > afile->afilesize) jackd->seek_end = afile->afilesize;
        }
        if (jackd->seek_end == 0 || ((jackd->playing_file == mainw->ascrap_file && !mainw->preview) && IS_VALID_CLIP(playfile)
                                     && mainw->files[playfile]->achans > 0)) jackd->seek_end = INT64_MAX;

        shrink_factor = (float)((double)jackd->sample_in_rate / (double)jackd->sample_out_rate / (double) mainw->audio_stretch);
        in_framesd = fabs((double)shrink_factor * (double)jackFramesAvailable);

        // add in a small random factor so on longer timescales we aren't losing or gaining samples
        in_bytes = (int)(in_framesd + fastrand_dbl(1.)) * jackd->num_input_channels
                   * jackd->bytes_per_channel;

        xin_bytes = (int)(in_framesd * jackd->num_input_channels * jackd->bytes_per_channel);

        /* in_bytes = ABS((in_frames = ((double)jackd->sample_in_rate / (double)jackd->sample_out_rate * */
        /*                              (double)jackFramesAvailable + ((double)fastrand() / (double)LIVES_MAXUINT64)))) */
        /*   * jackd->num_input_channels * jackd->bytes_per_channel; */

        // update looping mode
        if ((mainw->loop_cont || mainw->whentostop != STOP_ON_AUD_END) && !mainw->preview) {
          if (mainw->ping_pong && (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS)
              && ((prefs->audio_opts & AUDIO_OPTS_FOLLOW_CLIPS) || mainw->current_file == jackd->playing_file)
              && (!mainw->event_list || mainw->record || mainw->record_paused)
              && mainw->agen_key == 0 && !mainw->agen_needs_reinit
              && (!(prefs->audio_opts & AUDIO_OPTS_IS_LOCKED)
                  || ((prefs->audio_opts & AUDIO_OPTS_LOCKED_PING_PONG))))
            jackd->loop = AUDIO_LOOP_PINGPONG;
          else jackd->loop = AUDIO_LOOP_FORWARD;
        } else {
          jackd->loop = AUDIO_LOOP_NONE;
        }

        if (cache_buffer) eof = cache_buffer->eof;

        if ((shrink_factor = (float)in_framesd / (float)jackFramesAvailable / mainw->audio_stretch) >= 0.f) {
          jackd->seek_pos += in_bytes;
          if (jackd->playing_file != mainw->ascrap_file) {
            if (eof || (jackd->seek_pos >= jackd->seek_end && !afile->opening)) {
              if (jackd->loop == AUDIO_LOOP_NONE) {
                if (*jackd->whentostop == STOP_ON_AUD_END) {
                  *jackd->cancelled = CANCEL_AUD_END;
                  jackd->in_use = FALSE;
                }
                in_bytes = 0;
              } else {
                if (jackd->loop == AUDIO_LOOP_PINGPONG && ((jackd->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  jackd->sample_in_rate = -jackd->sample_in_rate;
                  afile->adirection = -afile->adirection;
                  jackd->seek_pos -= (jackd->seek_pos - jackd->seek_end);
                } else {
                  if (mainw->playing_sel) {
                    fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos
                                                     = (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                                                       * afile->achans * (afile->asampsize / 8);
                  } else fwd_seek_pos = jackd->seek_pos = jackd->real_seek_pos = 0;
                  if (mainw->record && !mainw->record_paused) jack_set_rec_avals(jackd);
		  // *INDENT-OFF*
		}}}}
	  // *INDENT-ON*
        } else {
          // reverse playback
          off_t seek_start = (mainw->playing_sel ?
                              (int64_t)((double)(afile->start - 1.) / afile->fps * afile->arps)
                              * afile->achans * (afile->asampsize / 8) : 0);
          seek_start = ALIGN_CEIL64(seek_start - qnt, qnt);

          if ((jackd->seek_pos -= in_bytes) < seek_start) {
            // reached beginning backwards
            if (jackd->playing_file != mainw->ascrap_file) {
              if (jackd->loop == AUDIO_LOOP_NONE) {
                if (*jackd->whentostop == STOP_ON_AUD_END) {
                  *jackd->cancelled = CANCEL_AUD_END;
                }
                jackd->in_use = FALSE;
              } else {
                if (jackd->loop == AUDIO_LOOP_PINGPONG && ((jackd->playing_file != mainw->playing_file)
                    || clip_can_reverse(mainw->playing_file))) {
                  jackd->sample_in_rate = -jackd->sample_in_rate;
                  afile->adirection = -afile->adirection;
                  shrink_factor = -shrink_factor;
                  jackd->seek_pos = seek_start;
                } else {
                  jackd->seek_pos += jackd->seek_end;
                  if (jackd->seek_pos > jackd->seek_end - in_bytes)
                    jackd->seek_pos = jackd->seek_end - in_bytes;
                }
              }
              fwd_seek_pos = jackd->real_seek_pos = jackd->seek_pos;
              if (mainw->record && !mainw->record_paused) jack_set_rec_avals(jackd);
            }
          }
        }

        if (jackd->mute || !cache_buffer ||
            (in_bytes == 0 &&
             ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) || mainw->multitrack || mainw->preview))) {
          if (!mainw->multitrack && cache_buffer && !wait_cache_buffer
              && ((mainw->agen_key == 0 && !mainw->agen_needs_reinit)
                  || mainw->preview)) {
            push_cache_buffer(cache_buffer, jackd, in_bytes, nframes, shrink_factor);
          }
          output_silence(0, nframes, jackd, out_buffer);
          if (jackd->playing_file >= 0) afile->aseek_pos = jackd->seek_pos;
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          in_ap = FALSE;
          return 0;
        } else {
          xin_bytes = 0;
        }
        if (mainw->agen_key != 0 && !mainw->multitrack && !mainw->preview) {
          // how much audio do we want to pull from any generator ?
          in_bytes = jackFramesAvailable * nch * 4;
          xin_bytes = in_bytes;
        }

        if (!jackd->in_use || in_bytes == 0) {
          // reached end of audio with no looping
          output_silence(0, nframes, jackd, out_buffer);

          jackd->is_silent = TRUE;

          if (jackd->seek_pos < 0. && jackd->playing_file > -1 && xfile) {
            jackd->seek_pos += (double)(jackd->sample_in_rate / jackd->sample_out_rate)
                               * nframes * xfile->achans * xfile->asampsize / 8;
          }
          lives_proc_thread_include_states(self, THRD_STATE_IDLING);
          lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
          in_ap = FALSE;
          return 0;
        }

        if (mainw->multitrack || mainw->preview || (mainw->agen_key == 0 && !mainw->agen_needs_reinit)) {
          inputFramesAvailable = cache_buffer->samp_space;
        } else {
          inputFramesAvailable = jackFramesAvailable;
        }

#ifdef DEBUG_AJACK
        lives_printerr("%ld inputFramesAvailable == %f, %d %d,jackFramesAvailable == %ld\n", inputFramesAvailable,
                       in_framesd, jackd->sample_in_rate, jackd->sample_out_rate, jackFramesAvailable);
#endif

        /* write as many bytes as we have space remaining, or as much as we have data to write */
        //numFramesToWrite = MIN(jackFramesAvailable, inputFramesAvailable);
        numFramesToWrite = (uint64_t)((double)inputFramesAvailable / (double)fabsf(shrink_factor) + .001);

#ifdef DEBUG_AJACK
        lives_printerr("nframes == %d, jackFramesAvailable == %ld,\n\tjackd->num_input_channels == %ld,"
                       "jackd->num_output_channels == %d, nf2w %ld, in_bytes %ld, sf %.8f\n",
                       nframes, jackFramesAvailable, jackd->num_input_channels, nch,
                       numFramesToWrite, in_bytes, shrink_factor);
#endif
        numFramesToWrite = jackFramesAvailable;
        jackd->frames_written += numFramesToWrite;
        //jackFramesAvailable -= numFramesToWrite; /* take away what was written */

#ifdef DEBUG_AJACK
        lives_printerr("jackFramesAvailable == %ld\n", jackFramesAvailable);
#endif
      }

      // playback from memory or file
      if (CLIP_HAS_AUDIO(jackd->playing_file) && !mainw->multitrack)
        vol = lives_vol_from_linear(future_prefs->volume * afile->vol);
      else vol = lives_vol_from_linear(future_prefs->volume);

      if (numFramesToWrite > 0) {
        if (!from_memory) {
          //	if (((int)(jackd->num_calls/100.))*100==jackd->num_calls) if (mainw->soft_debug) g_print("audio pip\n");
          if ((mainw->agen_key != 0 || mainw->agen_needs_reinit || cache_buffer->bufferf) && !mainw->preview_rendering &&
              !jackd->mute) { // TODO - try buffer16 instead of bufferf
            float *fbuffer = NULL;

            if (!mainw->preview && !mainw->multitrack && (mainw->agen_key != 0 || mainw->agen_needs_reinit)) {
              // audio generated from plugin
              if (mainw->agen_needs_reinit) pl_error = TRUE;
              else {
                if (!get_audio_from_plugin(out_buffer, nch,
                                           jackd->sample_out_rate, numFramesToWrite, TRUE)) {
                  pl_error = TRUE;
                }
                jackFramesAvailable = 0;
              }

              // get back non-interleaved float fbuffer; rate and channels should match
              if (pl_error) {
                // error in plugin, put silence
                output_silence(0, numFramesToWrite, jackd, out_buffer);
              } else {
                for (i = 0; i < nch; i++) {
                  // push non-interleaved audio in fbuffer to jack
                  if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT) {
                    // we will push the pre-effected audio to any audio reactive generators
                    append_to_audio_bufferf(out_buffer[i], numFramesToWrite, i == nch - 1 ? -i - 1 : i + 1);
                  }
                }
              }
              //}
              if (!pl_error && has_audio_filters(AF_TYPE_ANY)) {
                float **xfltbuf;
                ticks_t tc = mainw->currticks;
                // apply inplace any effects with audio in_channels, result goes to jack
                weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                weed_layer_set_audio_data(layer, out_buffer, jackd->sample_out_rate,
                                          nch, numFramesToWrite);
                weed_set_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
                weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                xfltbuf = weed_layer_get_audio_data(layer, NULL);
                for (i = 0; i < nch; i++) {
                  if (xfltbuf[i] != out_buffer[i]) {
                    lives_memcpy(out_buffer[i], xfltbuf[i], numFramesToWrite * sizeof(float));
                    lives_free(xfltbuf[i]);
                  }
                }
                lives_free(xfltbuf);
                weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                weed_layer_unref(layer);
              }

              pthread_mutex_lock(&mainw->vpp_stream_mutex);
              if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
                (*mainw->vpp->render_audio_frame_float)(out_buffer, numFramesToWrite);
              }
              pthread_mutex_unlock(&mainw->vpp_stream_mutex);

              if (mainw->record && mainw->ascrap_file != -1 && mainw->playing_file > 0) {
                // if recording we will save this audio fragment
                int out_unsigned = mainw->files[mainw->ascrap_file]->signed_endian & AFORM_UNSIGNED;
                rbytes = numFramesToWrite * mainw->files[mainw->ascrap_file]->achans *
                         mainw->files[mainw->ascrap_file]->asampsize >> 3;

                rbytes = audio_read_inner(jackd, out_buffer, mainw->ascrap_file, numFramesToWrite, 1.0,
                                          !(mainw->files[mainw->ascrap_file]->signed_endian
                                            & AFORM_BIG_ENDIAN), out_unsigned);

                mainw->files[mainw->ascrap_file]->aseek_pos += rbytes;
              }
            } else {
              // audio from a file
              // BAD - non realtime
              if (wait_cache_buffer) {
                while (!cache_buffer->is_ready && !cache_buffer->die) {
                  lives_nanosleep(LIVES_FORTY_WINKS);
                  if (mainw->is_exiting) cache_buffer->die = TRUE;
                }
                wait_cache_buffer = FALSE;
              }

              // BAD - non realtime
              pthread_mutex_lock(&mainw->cache_buffer_mutex);
              if (!cache_buffer->die) {
                inputFramesAvailable = in_bytes / (jackd->num_input_channels * (afile->asampsize >> 3));
                numFramesToWrite = (uint64_t)((double)inputFramesAvailable / (double)fabsf(shrink_factor) + .001);

                if (numFramesToWrite > jackFramesAvailable) {
#ifdef DEBUG_AJACK
                  lives_printerr("dropping last %ld samples\n", numFramesToWrite - jackFramesAvailable);
#endif
                } else if (numFramesToWrite < jackFramesAvailable) {
                  // because of rounding, occasionally we get a sample or two short. Here we duplicate the last samples
                  // so as not to jump to leave a zero filled gap
                  size_t lack = jackFramesAvailable - numFramesToWrite;
                  for (i = 0; i < nch; i++) {
                    lives_memcpy(out_buffer[i] + numFramesToWrite, out_buffer[i] + numFramesToWrite - lack, lack * 4);
                  }
                }

                numFramesToWrite = jackFramesAvailable;

                // push audio from cache_buffer to jack
                for (i = 0; i < nch; i++) {
                  /* if (afile->asampsize == 32) */
                  /*   sample_move_float_float(out_buffer[i], cache_buffer->bufferf[i], numFramesToWrite, 1., 1, 1.); */
                  /* else  */
                  // we use output_channels here since the audio has been resampled to this
                  jackd->abs_maxvol_heard = sample_move_d16_float(out_buffer[i], cache_buffer->buffer16[0] + i, numFramesToWrite,
                                            jackd->num_output_channels, afile->signed_endian
                                            & AFORM_UNSIGNED, FALSE, vol);
                  if (mainw->afbuffer && prefs->audio_src != AUDIO_SRC_EXT) {
                    // we will push the pre-effected audio to any audio reactive generators
                    append_to_audio_bufferf(out_buffer[i], numFramesToWrite, i == nch - 1 ? -i - 1 : i + 1);
                  }
                }
                pthread_mutex_unlock(&mainw->cache_buffer_mutex);

                jackFramesAvailable = 0;

                if (has_audio_filters(AF_TYPE_ANY) && jackd->playing_file != mainw->ascrap_file) {
                  float **xfltbuf;
                  ticks_t tc = mainw->currticks;
                  // apply inplace any effects with audio in_channels
                  weed_layer_t *layer = weed_layer_new(WEED_LAYER_TYPE_AUDIO);
                  weed_layer_set_audio_data(layer, out_buffer, jackd->sample_out_rate,
                                            nch, numFramesToWrite);
                  weed_set_boolean_value(layer, WEED_LEAF_HOST_KEEP_ADATA, WEED_TRUE);
                  weed_apply_audio_effects_rt(layer, tc, FALSE, TRUE);
                  xfltbuf = weed_layer_get_audio_data(layer, NULL);
                  for (i = 0; i < nch; i++) {
                    if (xfltbuf[i] != out_buffer[i]) {
                      lives_memcpy(out_buffer[i], xfltbuf[i], numFramesToWrite * sizeof(float));
                      lives_free(xfltbuf[i]);
                    }
                  }
                  lives_free(xfltbuf);
                  weed_layer_set_audio_data(layer, NULL, 0, 0, 0);
                  weed_layer_unref(layer);
                }

                // BAD - non realtime
                pthread_mutex_lock(&mainw->vpp_stream_mutex);
                if (mainw->ext_audio && mainw->vpp && mainw->vpp->render_audio_frame_float) {
                  (*mainw->vpp->render_audio_frame_float)(out_buffer, numFramesToWrite);
                }
                pthread_mutex_unlock(&mainw->vpp_stream_mutex);
              } else {
                // cache_buffer->die == TRUE
                pthread_mutex_unlock(&mainw->cache_buffer_mutex);
                output_silence(0, numFramesToWrite, jackd, out_buffer);
              }
            }

            if (jackd->astream_fd != -1) {
              // audio streaming if enabled
              unsigned char *xbuf;

              rbytes = numFramesToWrite * nch * 2;
              nbytes = rbytes << 1;

              if (pl_error) {
                // generator plugin error - output silence
                check_zero_buff(rbytes);
                audio_stream(zero_buff, rbytes, jackd->astream_fd);
              } else {
                if ((mainw->agen_key == 0 && !mainw->agen_needs_reinit) && !mainw->multitrack && !mainw->preview)
                  xbuf = (unsigned char *)cache_buffer->buffer16[0];
                else {
                  // plugin is generating and we are streaming: convert fbuffer to s16
                  float **fp = (float **)lives_calloc(nch, sizeof(float *));
                  for (i = 0; i < nch; i++) {
                    fp[i] = fbuffer + i;
                  }
                  xbuf = (unsigned char *)lives_calloc(nbytes * nch, 1);
                  sample_move_float_int((void *)xbuf, fp, numFramesToWrite, 1.0,
                                        nch, 16, 0, TRUE, TRUE, 1.0);
                }

                if (nch != 2) {
                  // need to remap channels to stereo (assumed for now)
                  size_t bysize = 4, tsize = 0;
                  unsigned char *inbuf, *oinbuf = NULL;

                  if ((mainw->agen_key != 0 || mainw->agen_needs_reinit) && !mainw->multitrack && !mainw->preview)
                    inbuf = (unsigned char *)cache_buffer->buffer16[0];
                  else oinbuf = inbuf = xbuf;

                  xbuf = (unsigned char *)lives_calloc(nbytes, 1);
                  if (!xbuf) {
                    // external streaming
                    rbytes = numFramesToWrite * nch * 2;
                    if (check_zero_buff(rbytes))
                      audio_stream(zero_buff, rbytes, jackd->astream_fd);
                    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
                    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
                    in_ap = FALSE;
                    return 0;
                  }
                  if (nch == 1) bysize = 2;
                  while (nbytes > 0) {
                    lives_memcpy(xbuf + tsize, inbuf, bysize);
                    tsize += bysize;
                    nbytes -= bysize;
                    if (bysize == 2) {
                      // duplicate mono channel
                      lives_memcpy(xbuf + tsize, inbuf, bysize);
                      tsize += bysize;
                      nbytes -= bysize;
                      inbuf += bysize;
                    } else {
                      // or skip extra channels
                      inbuf += nch * 4;
                    }
                  }
                  nbytes = numFramesToWrite * nch * 4;
                  lives_freep((void **)&oinbuf);
                }

                // push to stream
                rbytes = numFramesToWrite * nch * 2;
                audio_stream(xbuf, rbytes, jackd->astream_fd);
                if (((mainw->agen_key != 0 || mainw->agen_needs_reinit) && !mainw->multitrack
                     && !mainw->preview) || xbuf != (unsigned char *)cache_buffer->buffer16[0]) lives_free(xbuf);
              }
            } // end audio stream
            lives_freep((void **)&fbuffer);
          } else {
            // no generator plugin, but audio is muted
            output_silence(0, numFramesToWrite, jackd, out_buffer);
          }
        } else {
          // cached from files - multitrack mode
          if (jackd->read_abuf > -1 && !jackd->mute) {
            sample_move_abuf_float(out_buffer, nch, nframes, jackd->sample_out_rate, vol);

            if (jackd->astream_fd != -1) {
              // audio streaming if enabled
              unsigned char *xbuf = (unsigned char *)out_buffer;
              nbytes = numFramesToWrite * nch * 4;

              if (nch != 2) {
                // need to remap channels to stereo (assumed for now)
                size_t bysize = 4, tsize = 0;
                unsigned char *inbuf = (unsigned char *)out_buffer;
                xbuf = (unsigned char *)lives_calloc(nbytes, 1);
                if (!xbuf) {
                  output_silence(0, numFramesToWrite, jackd, out_buffer);
                  lives_proc_thread_include_states(self, THRD_STATE_IDLING);
                  lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
                  in_ap = FALSE;
                  return 0;
                }

                if (nch == 1) bysize = 2;
                while (nbytes > 0) {
                  lives_memcpy(xbuf + tsize, inbuf, bysize);
                  tsize += bysize;
                  nbytes -= bysize;
                  if (bysize == 2) {
                    // duplicate mono channel
                    lives_memcpy(xbuf + tsize, inbuf, bysize);
                    tsize += bysize;
                    nbytes -= bysize;
                    inbuf += bysize;
                  } else {
                    // or skip extra channels
                    inbuf += nch * 4;
                  }
                }
                nbytes = numFramesToWrite * nch * 2;
              }
              rbytes = numFramesToWrite * nch * 2;
              audio_stream(xbuf, rbytes, jackd->astream_fd);
              if (xbuf != (unsigned char *)out_buffer) lives_free(xbuf);
            }
          } else {
            // muted or no audio available
            output_silence(0, numFramesToWrite, jackd, out_buffer);
          }
        }
      } else {
        // no input frames left, pad with silence
        output_silence(nframes - jackFramesAvailable, jackFramesAvailable, jackd, out_buffer);
        jackFramesAvailable = 0;
      }
    }
    //

    if (!from_memory) {
      // push the cache_buffer to be filled
      if (!mainw->multitrack && !wait_cache_buffer && ((mainw->agen_key == 0 && ! mainw->agen_needs_reinit)
          || mainw->preview)) {
        push_cache_buffer(cache_buffer, jackd, in_bytes * 2., nframes, shrink_factor);
      }
      /// advance the seek pos even if we are reading from a generator
      /// audio gen outptut is float, so convert to playing file bytesize
      if (shrink_factor > 0.) jackd->seek_pos += xin_bytes / 4 * jackd->bytes_per_channel;
    }

    if (jackFramesAvailable > 0) {
#ifdef DEBUG_AJACK
      ++mainw->uflow_count;
      lives_printerr("buffer underrun of %ld frames\n", jackFramesAvailable);
#endif
      output_silence(nframes - jackFramesAvailable, jackFramesAvailable, jackd, out_buffer);
    }
  } else if (jackd->state == JackTransportStarting || jackd->state == JackTransportStopped ||
             jackd->state == JackTClosed || jackd->state == JackTReset) {
#ifdef DEBUG_AJACK
    lives_printerr("PAUSED or STOPPED or CLOSED, outputting silence\n");
#endif

    /* output silence if nothing is being outputted */
    output_silence(0, nframes, jackd, out_buffer);
    jackd->is_silent = TRUE;

    /* if we were told to reset then zero out some variables */
    /* and transition to STOPPED */
    if (jackd->state == JackTReset) {
      jackd->state = (jack_transport_state_t)JackTStopped; /* transition to STOPPED */
    }
  }

  if (jackd->playing_file >= 0) afile->aseek_pos = jackd->seek_pos;

#ifdef DEBUG_AJACK
  lives_printerr("done\n");
#endif

  lives_proc_thread_include_states(self, THRD_STATE_IDLING);
  lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
  in_ap = FALSE;
  return 0;
}


static int xrun_callback(void *arg) {
  jack_driver_t *jackd = (jack_driver_t *)arg;
  float delay = jack_get_xrun_delayed_usecs(jackd->client);
  volatile float *load = get_core_loadvar(0);
  if (prefs->show_dev_opts) {
    g_print("\n\nXRUN: %f %f\n", delay, *load);
    //g_print("\n\nXRUN: %f\n", delay);
  }
  mainw->xrun_active = TRUE;
  if (delay >= 0.)
    if (IS_VALID_CLIP(jackd->playing_file))
      jackd->seek_pos += (off_t)((double)jackd->sample_in_rate * ((double)delay / (double)MILLIONS(1))
                                 * afile->adirection * afile->achans
                                 * (afile->asampsize >> 3));
  return 0;
}


static void timebase_callback(jack_transport_state_t state, jack_nframes_t nframes, jack_position_t *pos, int new_pos,
                              void *arg) {
  jack_driver_t *jackd = (jack_driver_t *)arg;
  jack_transport_locate(jackd->client, (jack_nframes_t)((double)lives_jack_get_pos(jackd) *
                        (double)jackd->sample_out_rate));
}


static int start_ready_callback(jack_transport_state_t state, jack_position_t *pos, void *arg) {
  // mainw->video_seek_ready is generally FALSE
  // if we are not playing, the transport poll should start playing which will set set
  // mainw->video_seek_ready to true, as soon as the video is at the right place

  // if we are playing, we set mainw->scratch
  // this will either force a resync of audio in free playback
  // or reset the event_list position in multitrack playback

  jack_driver_t *jackd = (jack_driver_t *)arg;

  // go away until the app has started up properly
  if (mainw->go_away) return TRUE;

  if (!(prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE) || !(prefs->jack_opts & JACK_OPTS_ENABLE_TCLIENT)) return TRUE;

  if (!jackd->client) return TRUE;

  if (prefs->jack_opts & JACK_OPTS_STRICT_SLAVE) {
    if (!LIVES_IS_PLAYING && state == JackTransportStopped) {
      if (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE) {
        double trtime = (double)jack_transport_get_current_ticks(jackd) / TICKS_PER_SECOND_DBL;
        if (!mainw->multitrack) {
#ifndef ENABLE_GIW_3
          lives_ruler_set_value(LIVES_RULER(mainw->hruler), x);
          lives_widget_queue_draw_if_visible(mainw->hruler);
#else
          lives_adjustment_set_value(giw_timeline_get_adjustment(GIW_TIMELINE(mainw->hruler)), trtime);
#endif
        } else mt_tl_move(mainw->multitrack, trtime);
      }
      return TRUE;
    }
  }

  if (LIVES_IS_PLAYING && (prefs->jack_opts & JACK_OPTS_TIMEBASE_SLAVE)) {
    // trigger audio resync
    mainw->scratch = SCRATCH_JUMP;
  }

  return (mainw->video_seek_ready & mainw->audio_seek_ready);
}


static size_t audio_read_inner(jack_driver_t *jackd, float **in_buffer, int ofileno, int nframes,
                               double out_scale, boolean rev_endian, boolean out_unsigned) {
  return 0;
}

size_t jack_write_data(float out_scale, int achans, int fileno, size_t nframes, float **in_buffer) {
  return 0;
  lives_clip_t *ofile;
  void *holding_buff, *holding_buff2;;
  size_t target_bytes;
  ssize_t actual_bytes;
  int64_t frames_out;

  boolean is_float = FALSE;
  boolean rev_endian = FALSE;
  boolean out_unsigned;

  int sampsize;
  int swap_sign;

  if (THREADVAR(bad_aud_file)) return 0;
  if (mainw->rec_samples == 0) return 0;
  if (nframes == 0) return 0;
  if (!IS_VALID_CLIP(fileno)) return 0;

  ofile = mainw->files[fileno];
  sampsize = ofile->asampsize >> 3;

  if (prefs->audio_opts & AUDIO_OPTS_AUX_RECORD) achans <<= 1;

  frames_out = (int64_t)((double)nframes / out_scale + .49999);
  holding_buff = lives_calloc(frames_out, achans * sampsize);

  if (!holding_buff) return 0;

  out_unsigned = ofile->signed_endian & AFORM_UNSIGNED;

  if (!is_float) {
    if (ofile->asampsize == 16) {
      int aendian = !(ofile->signed_endian & AFORM_BIG_ENDIAN);
      if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
          || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
        rev_endian = TRUE;
    }
    frames_out = sample_move_float_int(holding_buff, in_buffer, frames_out, out_scale, achans,
                                       ofile->asampsize, out_unsigned, rev_endian, FALSE, 1.);
  } else
    frames_out = float_interleave(holding_buff, in_buffer, frames_out, out_scale, achans, 1.);

  frames_out /= achans;

  if (mainw->rec_samples > 0) {
    if (frames_out > mainw->rec_samples * achans) frames_out = mainw->rec_samples * achans;
    mainw->rec_samples -= frames_out / achans;
  }
  // for 16bit, generally we use S16, so if we want U16, we should change it
  swap_sign = ofile->signed_endian & AFORM_UNSIGNED;

  if (ofile->asampsize == 16) {
    int aendian = !(ofile->signed_endian & AFORM_BIG_ENDIAN);
    if ((aendian && (capable->hw.byte_order == LIVES_BIG_ENDIAN))
        || (!aendian && (capable->hw.byte_order == LIVES_LITTLE_ENDIAN)))
      rev_endian = TRUE;
  }

  target_bytes = frames_out * ofile->achans * (ofile->asampsize >> 3);

  holding_buff2 = lives_malloc(target_bytes * 4);
  if (!holding_buff2) {
    lives_free(holding_buff);
    return 0;
  }

  if (ofile->asampsize == 16) {
    sample_move_d16_d16((short *)holding_buff2, holding_buff, frames_out, target_bytes, 1., ofile->achans, achans,
                        rev_endian ? SWAP_L_TO_X : 0, swap_sign ? SWAP_S_TO_U : 0);
  } else {
    sample_move_d16_d8((uint8_t *)holding_buff2, holding_buff, frames_out, target_bytes, 1., ofile->achans, achans,
                       swap_sign ? SWAP_S_TO_U : 0);
  }

  if (mainw->rec_samples > 0) {
    if (frames_out > mainw->rec_samples) frames_out = mainw->rec_samples;
    mainw->rec_samples -= frames_out;
  }

  actual_bytes = lives_write_buffered(mainw->aud_rec_fd, holding_buff2, target_bytes, TRUE);

  if (actual_bytes > 0) {
    uint64_t chk = (mainw->aud_data_written & AUD_WRITE_CHECK);
    mainw->aud_data_written += actual_bytes;
    if (fileno == mainw->ascrap_file) add_to_ascrap_mb(actual_bytes);
    check_for_disk_space((mainw->aud_data_written & AUD_WRITE_CHECK) != chk);
    ofile->aseek_pos += actual_bytes;
  }
  if (actual_bytes < target_bytes) THREADVAR(bad_aud_file) = filename_from_fd(NULL, mainw->aud_rec_fd);

  //if (holding_buff != data)
  lives_free(holding_buff2);
  lives_free(holding_buff);

  return actual_bytes;
}


static float **back_buff = NULL;

static int audio_read(jack_nframes_t nframes, void *arg) {
  // read nframes from jack buffer, and then write to mainw->aud_rec_fd

  // this is the jack callback for when we are recording audio

  // for AUDIO_SRC_EXT, jackd->playing_file is actually the file we write audio to
  // which can be either the ascrap file (for playback recording),
  // or a normal file (for voiceovers), or -1 (just listening)

  // TODO - get abs_maxvol_heard
  GET_PROC_THREAD_SELF(self);
  jack_driver_t *jackd = (jack_driver_t *)arg;
  float *in_buffer[jackd->num_input_channels];
  float tval = 0;
  size_t rbytes = 0;
  int nch = jackd->num_input_channels;
  int i;

  static lives_thread_data_t *tdata = NULL;

  if (!tdata) {
    tdata = get_thread_data();
    // pulsed->inst will be our aplayer object instance
    // as a stopgap, we can treat the aplayer instance as a lives_proc_thread
    // in the sense that it runs this function, but by being "queued" by an external entity
    self = jackd->inst;
    lives_thread_switch_self(self, FALSE);
    lives_snprintf(tdata->vars.var_origin, 128, "%s", "pulseaudio writer Thread");
    lives_proc_thread_include_states(self, THRD_STATE_EXTERN);
  }

  lives_proc_thread_include_states(self, THRD_STATE_RUNNING);
  lives_proc_thread_exclude_states(self, THRD_STATE_IDLING);

  lives_hooks_async_join(NULL, DATA_READY_HOOK);

  if (!jackd->in_use || (mainw->playing_file < 0 && prefs->audio_src == AUDIO_SRC_EXT)
      || mainw->effects_paused || mainw->rec_samples == 0) {
    lives_proc_thread_include_states(self, THRD_STATE_IDLING);
    lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);
    return 0;
  }

  // deal with aux first; we need to be either recording or monitoring it
  if (((prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
       || ((prefs->audio_opts & AUDIO_OPTS_AUX_RECORD)
           && (mainw->record && !mainw->record_paused)))
      && mainw->audio_frame_buffer_aux) {
    for (i = 0; i < nch; i++) {
      in_buffer[nch + i] = (float *)jack_port_get_buffer(jackd->input_port[nch + i], nframes);
      append_to_aux_audio_bufferf(in_buffer[nch + i], nframes, i);
    }
    mainw->audio_frame_buffer_aux->samples_filled += nframes;
    // TODO - if recording aux, do so
  }

  for (i = 0; i < nch; i++) {
    in_buffer[i] = (float *)jack_port_get_buffer(jackd->input_port[i], nframes);
    tval += *in_buffer[i];
  }

  if (!mainw->fs && !mainw->faded && !mainw->multitrack && mainw->ext_audio_mon)
    lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_mon), tval > 0.);

  jackd->frames_read += nframes;

  // recording: what we record:-
  // external audio, provided we are not applying fx to it it (handled in writer client)
  // aux audio - will be mixed with whatever is playing and saved there

  if (!back_buff) back_buff = lives_calloc(jackd->num_input_channels, sizeof(float *));
  for (int cc = 0; cc < jackd->num_input_channels; cc++) {
    if (back_buff[cc]) lives_free(back_buff[cc]);
    back_buff[cc] = lives_malloc(nframes * 4);
    lives_memcpy(back_buff[cc], in_buffer[cc], nframes * 4);
  }

  lives_aplayer_set_data_len(jackd->inst, nframes);
  lives_aplayer_set_data(jackd->inst, (void *)back_buff);

  lives_hooks_trigger(lives_proc_thread_get_hook_stacks(self), DATA_READY_HOOK);

  rbytes = nframes * jackd->num_input_channels * 4;
  jackd->seek_pos += rbytes;

  if (mainw->rec_samples == 0 && mainw->cancelled == CANCEL_NONE)
    mainw->cancelled = CANCEL_KEEP; // we wrote the required #
  lives_proc_thread_include_states(self, THRD_STATE_IDLING);
  lives_proc_thread_exclude_states(self, THRD_STATE_RUNNING);

  return 0;
}


static void jack_reset_driver(jack_driver_t *jackd) {
  // this a custom transport state
  jackd->state = (jack_transport_state_t)JackTReset;
}


void jack_close_client(jack_driver_t *jackd) {
  //lives_printerr("closing the jack client thread\n");
  if (jackd->client) {
    if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_WRITER)
      if (cache_buffer) cache_buffer->die = TRUE;
    jack_deactivate(jackd->client);
    jack_set_process_callback(jackd->client, NULL, jackd);
    lives_nanosleep_while_true(in_ap);
    jack_client_close(jackd->client);
  }

  jack_reset_driver(jackd);
  jackd->client = NULL;

  jackd->is_active = FALSE;
}


// create a new client but don't connect the ports yet
boolean jack_create_client_writer(jack_driver_t *jackd) {
  // WARNING = this function is run as a thread with a timeout and guilotine
  // it must return within 2 seconds (LIVES_SHORTEST_TIMEOUT)
  // else it will be terminated
  // if the function is going to block for a known reason, e.g getting driver list frim user then
  // - obtain the mointoring thread (dispatcher) ?, include the THRD_STATE_BUSY state
  // lives_proc_thread_include_states(). This will have the effect of causing the timout to be continuously reset
  // After return, clear the BUSY state with lives_proc_thread_exclude_states(), which will retstart the timer
  GET_PROC_THREAD_SELF(self);
  char portname[32];
  char *logmsg;
  boolean is_test = FALSE;
  int nch = jackd->num_output_channels;

  if (mainw->aplayer_broken) return FALSE;

  if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) is_test = TRUE;

  if (!prefs->startup_phase || is_test) {
    jackd->is_active = FALSE;
    if (!lives_jack_init(JACK_CLIENT_TYPE_AUDIO_WRITER, jackd)) {
      if (is_test) finish_test(jackd, FALSE, FALSE, FALSE);
      return FALSE;
    }

    jackd->sample_out_rate = jackd->sample_in_rate = jack_get_sample_rate(jackd->client);

    logmsg = lives_strdup_printf("Server sample rate is %d Hz\n", jackd->sample_out_rate);
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);

    if (prefs->startup_phase) {
      jackd->jack_port_flags |= JackPortIsInput;
      if (in_ports) lives_free(in_ports);
      in_ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    }

    if (is_test) {
      // test reader first
      //finish_test(jackd, TRUE, FALSE, FALSE);
      return TRUE;
    }
  }

  if (self) lives_proc_thread_include_states(self, THRD_STATE_BUSY);

  for (int i = 0; i < nch; i++) {
    if (!i) {
      lives_snprintf(portname, 32, "front-left");
    } else if (i == 1) {
      lives_snprintf(portname, 32, "front-right");
    } else lives_snprintf(portname, 32, "out_%d", i);

#ifdef DEBUG_JACK_PORTS
    lives_printerr("output port %d is named '%s'\n", i, portname);
#endif

    jackd->output_port[i] =
      jack_port_register(jackd->client, portname, JACK_DEFAULT_AUDIO_TYPE,
                         JackPortIsOutput | JackPortIsTerminal, 0);

    if (!jackd->output_port[i]) {
      lives_printerr("no more JACK output ports available\n");
      return FALSE;
    }
    jackd->out_ports_available++;
  }

#if 0
  if ((prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
      || (prefs->audio_opts & AUDIO_OPTS_AUX_RECORD)) {
    for (int i = 0; i < nch; i++) {
      if (!i) {
        lives_snprintf(portname, 32, "aux_L");
      } else if (i == 1) {
        lives_snprintf(portname, 32, "aux_R");
      } else lives_snprintf(portname, 32, "aux_out_%d", i);

#ifdef DEBUG_JACK_PORTS
      lives_printerr("output port %d is named '%s'\n", i, portname);
#endif

      jackd->output_port[nch + i] =
        jack_port_register(jackd->client, portname, JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsOutput | JackPortIsTerminal, 0);

      if (!jackd->output_port[nch + i]) {
        lives_printerr("no more JACK output ports available\n");
        return FALSE;
      }
      //jackd->out_ports_available++;
    }
  }
#endif

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(jackd->client, jack_shutdown, jackd);

  return TRUE;
}


boolean jack_create_client_reader(jack_driver_t *jackd) {
  // open a device to read audio from jack
  GET_PROC_THREAD_SELF(self);
  char portname[32];
  int nch = jackd->num_input_channels;
  boolean is_test = FALSE;
  boolean res;

  if (mainw->aplayer_broken) return FALSE;

  if (future_prefs->jack_opts & JACK_INFO_TEST_SETUP) is_test = TRUE;

  if (!prefs->startup_phase || is_test) {
    jackd->is_active = FALSE;

    // create a client and attach it to the server
    res = lives_jack_init(JACK_CLIENT_TYPE_AUDIO_READER, jackd);

    if (prefs->startup_phase) {
      jackd->jack_port_flags |= JackPortIsOutput;
      if (out_ports) lives_free(out_ports);
      out_ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
    }

    if (is_test) {
      finish_test(jackd, res, FALSE, TRUE);
      return res;
    }
    if (!res) return FALSE;
  }

  if (self) lives_proc_thread_include_states(self, THRD_STATE_BUSY);

  jackd->sample_in_rate = jackd->sample_out_rate = jack_get_sample_rate(jackd->client);
  //lives_printerr (lives_strdup_printf("engine sample rate: %ld\n",jackd->sample_rate));

  // create ports for the client (left and right channels)
  for (int i = 0; i < nch; i++) {
    if (!i) {
      lives_snprintf(portname, 32, "front-left");
    } else if (i == 1) {
      lives_snprintf(portname, 32, "front-right");
    } else lives_snprintf(portname, 32, "in_%d", i);
#define DEBUG_JACK_PORTS
#ifdef DEBUG_JACK_PORTS
    lives_printerr("input port %d is named '%s'\n", i, portname);
#endif
    jackd->input_port[i] =
      jack_port_register(jackd->client, portname, JACK_DEFAULT_AUDIO_TYPE,
                         JackPortIsInput | JackPortIsTerminal, 0);
    if (!jackd->input_port[i]) {
      lives_printerr("no more JACK input ports available\n");
      //return FALSE;
    }
  }

  if ((prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
      || (prefs->audio_opts & AUDIO_OPTS_AUX_RECORD)) {
    for (int i = 0; i < nch; i++) {
      if (!i) {
        lives_snprintf(portname, 32, "aux_L");
      } else if (i == 1) {
        lives_snprintf(portname, 32, "aux_R");
      } else lives_snprintf(portname, 32, "aux_in_%d", i);

#ifdef DEBUG_JACK_PORTS
      lives_printerr("input port %d is named '%s'\n", i, portname);
#endif
      jackd->input_port[nch + i] =
        jack_port_register(jackd->client, portname, JACK_DEFAULT_AUDIO_TYPE,
                           JackPortIsInput | JackPortIsTerminal, 0);
      if (!jackd->input_port[nch + i]) {
        lives_printerr("no more JACK input ports available\n");
        break;
      }
    }
  }

  /* tell the JACK server to call `srate()' whenever
     the sample rate of the system changes. */
  jack_set_sample_rate_callback(jackd->client, jack_get_srate, jackd);

  /* tell the JACK server to call `jack_shutdown()' if
     it ever shuts down, either entirely, or if it
     just decides to stop calling us. */
  jack_on_shutdown(jackd->client, jack_shutdown, jackd);

  if (!jackd->inst) jackd->inst = lives_player_inst_create(PLAYER_SUBTYPE_AUDIO);
  lives_aplayer_set_source(jackd->inst, AUDIO_SRC_EXT);
  lives_aplayer_set_achans(jackd->inst, jackd->num_input_channels);
  lives_aplayer_set_arate(jackd->inst, jackd->sample_in_rate);
  lives_aplayer_set_sampsize(jackd->inst, jackd->sample_in_rate);
  lives_aplayer_set_float(jackd->inst, TRUE);
  lives_aplayer_set_interleaved(jackd->inst, FALSE);

  return 0;
}


boolean jack_write_client_activate(jack_driver_t *jackd) {
  // connect client and activate it
  char **pieces;
  char *logmsg;
  int nch = jackd->num_output_channels;
  int i, j = 0;

  if (jackd->is_active) return TRUE; // already running

  jack_set_process_callback(jackd->client, audio_process, jackd);
  jack_set_xrun_callback(jackd->client, xrun_callback, jackd);

  /* tell the JACK server that we are ready to roll */
  if (jack_activate(jackd->client)) {
    // let's hope IT is too...
    logmsg = lives_strdup_printf("LiVES : Could not activate jack writer client");
    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    return FALSE;
  }

  // we are looking for input ports to connect to
  jackd->jack_port_flags |= JackPortIsInput;
  if (in_ports) lives_free(in_ports);
  in_ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);

  if (!in_ports) {
    logmsg = lives_strdup_printf("LiVES : No jack input ports available");
    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    return FALSE;
  }

  for (i = 0; in_ports[i]; i++);
  jackd->in_ports_available = i;

  /* connect the ports. Note: you can't do this before
     the client is activated (this may change in the future). */
  for (i = 0; i < jackd->in_ports_available; i++) {
#ifdef DEBUG_JACK_PORTS
    lives_printerr("found port %s\n", in_ports[i]);
#endif
    pieces = lives_strsplit(in_ports[i], ":", 2);
    if (lives_strcmp(pieces[0], prefs->jack_outport_client)) {
      lives_strfreev(pieces);
      continue;
    }
    logmsg = lives_strdup_printf("LiVES : found port %s belonging to %s. Will connect %s to it\n", pieces[1], pieces[0],
                                 jack_port_name(jackd->output_port[j]));


    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
#ifdef DEBUG_JACK_PORTS
    lives_printerr("client is %s, connecting to out port %s\n", pieces[0], jack_port_name(jackd->output_port[j]));
#endif
    if (jack_connect(jackd->client, jack_port_name(jackd->output_port[j]), in_ports[i])) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("connection failed\n");
#endif
      logmsg = lives_strdup_printf("LiVES : Could not connect to port");
      jack_log_errmsg(jackd, logmsg);
      LIVES_ERROR(logmsg);
      lives_free(logmsg);
      jack_log_errmsg(jackd, NULL);
      break;
    }
    lives_strfreev(pieces);
    j++;
  }

  if (j < nch) {
    logmsg = lives_strdup_printf("LiVES : failed to auto connect all output ports");
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
  }

  jackd->num_output_channels = j;

  // start using soundcard as timer source
  prefs->force_system_clock = FALSE;

  jackd->is_active = TRUE;
  jackd->jackd_died = FALSE;
  jackd->in_use = FALSE;
  jackd->is_paused = FALSE;

  d_print(_("Started jack audio subsystem.\n"));

  return TRUE;
}


boolean jack_read_client_activate(jack_driver_t *jackd, boolean autocon) {
  // connect driver for reading
  char **pieces;
  char *logmsg;
  int nch = jackd->num_input_channels;
  int i, j = 0;

  if (!jackd->is_active) {
    jack_set_process_callback(jackd->client, audio_read, jackd);
    if (jack_activate(jackd->client)) {
      logmsg = lives_strdup_printf("LiVES : Could not activate jack reader client");
      jack_log_errmsg(jackd, logmsg);
      LIVES_ERROR(logmsg);
      lives_free(logmsg);
      return FALSE;
    }
  }

  if (!autocon && (prefs->jack_opts & JACK_OPTS_NO_READ_AUTOCON)) goto jackreadactive;

  // we are looking for output ports to connect to
  jackd->jack_port_flags |= JackPortIsOutput;

  if (out_ports) lives_free(out_ports);
  out_ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);

  if (!out_ports) {
    logmsg = lives_strdup_printf("LiVES : No jack output ports available");
    jack_log_errmsg(jackd, logmsg);
    LIVES_ERROR(logmsg);
    lives_free(logmsg);
    return FALSE;
  }

  for (i = 0; out_ports[i]; i++);

  jackd->out_ports_available = i;

  /* connect the ports. Note: you can't do this before
     the client is activated (this may change in the future). */
  for (i = 0; i < jackd->out_ports_available; i++) {
#define DEBUG_JACK_PORTS
#ifdef DEBUG_JACK_PORTS
    lives_printerr("found port %s\n", out_ports[i]);
#endif
    pieces = lives_strsplit(out_ports[i], ":", 2);
    if (lives_strcmp(pieces[0], prefs->jack_inport_client)) {
      lives_strfreev(pieces);
      continue;
    }
    logmsg = lives_strdup_printf("LiVES : found port %s belonging to %s. Will connect %s to it\n", pieces[1], pieces[0],
                                 jack_port_name(jackd->input_port[j]));
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
#ifdef DEBUG_JACK_PORTS
    lives_printerr("client is %s, connecting to in port %s\n", pieces[0], jack_port_name(jackd->input_port[j]));
#endif
    if (jack_connect(jackd->client, out_ports[i], jack_port_name(jackd->input_port[j]))) {
#ifdef DEBUG_JACK_PORTS
      lives_printerr("connection failed\n");
#endif
      logmsg = lives_strdup_printf("LiVES : Could not connect to port");
      jack_log_errmsg(jackd, logmsg);
      LIVES_ERROR(logmsg);
      lives_free(logmsg);
      jack_log_errmsg(jackd, NULL);
      break;
    }
    lives_strfreev(pieces);
    j++;
  }

  if (j < nch) {
    logmsg = lives_strdup_printf("LiVES : failed to auto connect all input ports");
    jack_log_errmsg(jackd, logmsg);
    lives_free(logmsg);
  }

  if (!(future_prefs->jack_opts & JACK_INFO_TEST_SETUP))
    jackd->num_input_channels = j;

  // do we need to be connected for this ?
  // start using soundcard as timer source
  //prefs->force_system_clock = FALSE;

jackreadactive:

  jackd->is_active = TRUE;
  jackd->jackd_died = FALSE;
  jackd->in_use = FALSE;
  jackd->is_paused = FALSE;
  jackd->nframes_start = 0;
  d_print(_("Started jack audio reader.\n"));

  // start using soundcard as timer source
  prefs->force_system_clock = FALSE;

  return TRUE;
}


jack_driver_t *jack_get_driver(int dev_idx, boolean is_output) {
  jack_driver_t *jackd;

  if (is_output) jackd = &outdev[dev_idx];
  else jackd = &indev[dev_idx];
#ifdef TRACE_getReleaseDevice
  lives_printerr("dev_idx is %d\n", dev_idx);
#endif

  return jackd;
}


int jack_audio_init(void) {
  // initialise variables
  int i, j;
  jack_driver_t *jackd;

  for (i = 0; i < JACK_MAX_OUTDEVICES; i++) {
    jackd = &outdev[i];
    //jack_reset_dev(i, TRUE);
    jackd->dev_idx = i;
    jackd->client = NULL;
    jackd->in_use = FALSE;
    for (j = 0; j < JACK_MAX_PORTS; j++) jackd->volume[j] = 1.0f;
    jackd->state = (jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate = jackd->sample_in_rate = 0;
    jackd->seek_pos = jackd->seek_end = jackd->real_seek_pos = 0;
    jackd->msgq = NULL;
    jackd->num_calls = 0;
    jackd->astream_fd = -1;
    jackd->abs_maxvol_heard = 0.;
    jackd->jackd_died = FALSE;
    jackd->num_output_channels = MAX_ACHANS;
    jackd->mute = FALSE;
    jackd->is_silent = FALSE;
    jackd->out_ports_available = 0;
    jackd->read_abuf = -1;
    jackd->playing_file = -1;
    jackd->frames_written = jackd->frames_read = 0;
    *jackd->status_msg = 0;
  }
  return 0;
}


int jack_audio_read_init(void) {
  int i, j;
  jack_driver_t *jackd;

  for (i = 0; i < JACK_MAX_INDEVICES; i++) {
    jackd = &indev[i];
    //jack_reset_dev(i, FALSE);
    jackd->dev_idx = i;
    jackd->client = NULL;
    jackd->in_use = FALSE;
    for (j = 0; j < JACK_MAX_PORTS; j++) jackd->volume[j] = 1.0f;
    jackd->state = (jack_transport_state_t)JackTClosed;
    jackd->sample_out_rate = jackd->sample_in_rate = 0;
    jackd->seek_pos = jackd->seek_end = jackd->real_seek_pos = 0;
    jackd->msgq = NULL;
    jackd->num_calls = 0;
    jackd->astream_fd = -1;
    jackd->abs_maxvol_heard = 0.;
    jackd->jackd_died = FALSE;
    jackd->num_input_channels = MAX_ACHANS;
    jackd->mute = FALSE;
    jackd->in_ports_available = 0;
    jackd->playing_file = -1;
    jackd->frames_written = jackd->frames_read = 0;
    *jackd->status_msg = 0;
  }
  return 0;
}


volatile aserver_message_t *jack_get_msgq(jack_driver_t *jackd) {
  if (jackd->jackd_died || mainw->aplayer_broken) return NULL;
  return jackd->msgq;
}


void jack_time_reset(jack_driver_t *jackd, int64_t offset) {
  jackd->nframes_start = jack_frame_time(jackd->client) + (jack_nframes_t)(((double)offset / USEC_TO_TICKS) *
                         ((double)(jackd->client_type
                                   == JACK_CLIENT_TYPE_AUDIO_READER
                                   ? jackd->sample_in_rate
                                   : jackd->sample_out_rate) / 1000000.));
  jackd->frames_written = jackd->frames_read = 0;
  mainw->currticks = offset;
  mainw->startticks = 0;
}


ticks_t lives_jack_get_time(jack_driver_t *jackd) {
  // get the time in ticks since playback started
  volatile aserver_message_t *msg = jackd->msgq;
  static jack_nframes_t last_frames = 0;
  jack_nframes_t frames, retframes;
  double srate;

  if (!jackd->client) return -1;

  if (msg && msg->command == ASERVER_CMD_FILE_SEEK) {
    if (await_audio_queue(LIVES_DEFAULT_TIMEOUT)) return -1;
  }

  if (!jackd->client) return -1;
  frames = jack_frame_time(jackd->client);
  if (!jackd->client) return -1;
  srate = (double)(jackd->client_type == JACK_CLIENT_TYPE_AUDIO_READER
                   ? jackd->sample_in_rate : jackd->sample_out_rate);
  if (!jackd->client) return -1;

  retframes = frames;
  if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_WRITER) {
    if (last_frames > 0 && frames <= last_frames) {
      retframes += jackd->frames_written;
    } else jackd->frames_written = 0;
  } else {
    if (last_frames > 0 && frames <= last_frames) {
      retframes += jackd->frames_read;
    } else jackd->frames_read = 0;
  }
  last_frames = frames;
  return (double)(retframes - jackd->nframes_start) * (1000000. / srate) * USEC_TO_TICKS;
}


off_t lives_jack_get_offset(jack_driver_t *jackd) {
  // get current time position (seconds) in audio file
  if (jackd->playing_file > -1) return jackd->seek_pos;
  return -1;
}


double lives_jack_get_pos(jack_driver_t *jackd) {
  // get current time position (seconds) in audio file
  if (jackd->playing_file > -1)
    return fwd_seek_pos / (double)(afile->arate * afile->achans * afile->asampsize / 8);
  // from memory
  if (jackd->client_type == JACK_CLIENT_TYPE_AUDIO_WRITER)
    return (double)jackd->frames_written / (double)jackd->sample_out_rate;
  else
    return (double)jackd->frames_read / (double)jackd->sample_in_rate;
}


boolean jack_audio_seek_frame_velocity(jack_driver_t *jackd, double frame, double vel) {
  // seek to frame "frame" in current audio file
  // position will be adjusted to (floor) nearest sample

  volatile aserver_message_t *jmsg;
  int64_t seekstart;
  ticks_t timeout;
  double thresh = 0., delta = 0.;
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_DEFAULT_TIMEOUT);

  if (alarm_handle == ALL_USED) return FALSE;

  if (frame < 1) frame = 1;

  do {
    jmsg = jack_get_msgq(jackd);
  } while ((timeout = lives_alarm_check(alarm_handle)) > 0 && jmsg && jmsg->command != ASERVER_CMD_FILE_SEEK);
  lives_alarm_clear(alarm_handle);
  if (timeout == 0 || jackd->playing_file == -1) {
    return FALSE;
  }

  if (frame > afile->frames && afile->frames != 0) frame = afile->frames;
  seekstart = (int64_t)((double)(frame - 1.) / afile->fps * afile->arps) * afile->achans * (afile->asampsize / 8);
  if (cache_buffer) {
    delta = (double)(seekstart - lives_buffered_offset(cache_buffer->_fd)) / (double)(afile->arps * afile->achans *
            (afile->asampsize / 8));
    thresh = 1. / (double)afile->fps;
  }
  if (delta >= thresh || delta <= -thresh)
    jack_audio_seek_bytes(jackd, seekstart, afile);
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean jack_audio_seek_frame(jack_driver_t *jackd, double frame) {
  return jack_audio_seek_frame_velocity(jackd, frame, 0.);
}


off_t jack_audio_seek_bytes_velocity(jack_driver_t *jackd, off_t bytes, lives_clip_t *sfile, double vel) {
  // seek to position "bytes" in current audio file
  // position will be adjusted to (floor) nearest sample

  // if the position is > size of file, we will seek to the end of the file

  off_t seekstart;

  bytes = ALIGN_CEIL64(bytes, sfile->achans * (sfile->asampsize >> 3));
  jackd->seek_pos = fwd_seek_pos = bytes;

  seek_err = FALSE;

  if (0 && LIVES_IS_PLAYING && !mainw->preview) {
    jack_message2.tc = lives_get_current_ticks();
    jack_message2.command = ASERVER_CMD_FILE_SEEK_ADJUST;
  } else jack_message2.command = ASERVER_CMD_FILE_SEEK;

  if (jackd->in_use) {
    if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT) || jackd->playing_file == -1) {
      if (jackd->playing_file > -1) LIVES_WARN("Jack connect timed out");
      seek_err = TRUE;
      return 0;
    }
  }

  seekstart = ((int64_t)(bytes / sfile->achans / (sfile->asampsize / 8))) * sfile->achans * (sfile->asampsize / 8);

  if (seekstart < 0) seekstart = 0;
  if (seekstart > sfile->afilesize) seekstart = sfile->afilesize;
  jack_message2.next = NULL;
  jack_message2.data = lives_strdup_printf("%"PRId64, seekstart);
  if (vel !=  0.) jack_message2.extra = lives_strdup_printf("%f", vel);
  else lives_freep((void **)&jack_message2.extra);
  if (!jackd->msgq) jackd->msgq = &jack_message2;
  else jackd->msgq->next = &jack_message2;
  return seekstart;
}


LIVES_GLOBAL_INLINE off_t jack_audio_seek_bytes(jack_driver_t *jackd, int64_t bytes, lives_clip_t *sfile) {
  return jack_audio_seek_bytes_velocity(jackd, bytes, sfile, 0.);
}

boolean jack_try_reconnect(void) {
  jack_audio_init();
  jack_audio_read_init();

  mainw->jackd = jack_get_driver(0, TRUE);
  if (!jack_create_client_writer(mainw->jackd)) goto err123;

  mainw->jackd_read = jack_get_driver(0, FALSE);
  jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);

  d_print(_("\nConnection to jack audio was reset.\n"));
  return TRUE;

err123:
  mainw->aplayer_broken = TRUE;
  mainw->jackd = mainw->jackd_read = NULL;
  do_jack_lost_conn_error();
  return FALSE;
}


void jack_pb_end(void) {
  cache_buffer = NULL;
  if (prefs->audio_opts & AUDIO_OPTS_AUX_PLAY)
    unregister_aux_audio_channels(1);
  if (AUD_SRC_EXTERNAL && (prefs->audio_opts & AUDIO_OPTS_EXT_FX))
    unregister_audio_client(FALSE);
}


void jack_aud_pb_ready(jack_driver_t *jackd, int fileno) {
  // TODO - can we merge with switch_audio_clip()

  // prepare to play file fileno
  // - set loop mode
  // - check if we need to reconnect
  // - set vals

  // called at pb start and rec stop (after rec_ext_audio)
  lives_clip_t *sfile;
  char *tmpfilename = NULL;
  if (!jackd || !IS_VALID_CLIP(fileno)) return;

  avsync_force();

  sfile = mainw->files[fileno];
  if ((!mainw->multitrack || mainw->multitrack->is_rendering) &&
      (!mainw->event_list || mainw->record || (mainw->preview && mainw->is_processing))) {
    // tell jack server to open audio file and start playing it
    if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) seek_err = TRUE;
    else {
      jack_message.command = ASERVER_CMD_FILE_OPEN;
      jack_message.data = lives_strdup_printf("%d", fileno);
      jack_message.next = NULL;
      jackd->msgq = &jack_message;

      sfile = mainw->files[fileno];

      if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
          (sfile->laudio_time > 0. || sfile->opening ||
           (mainw->multitrack && mainw->multitrack->is_rendering &&
            lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
        lives_jack_set_client_attributes(jackd, fileno, TRUE, FALSE);
      }
      jack_audio_seek_bytes(jackd, sfile->aseek_pos, sfile);
    }

    if (seek_err) {
      seek_err = FALSE;
      if (jack_try_reconnect()) jack_audio_seek_bytes(jackd, sfile->aseek_pos, sfile);
      if (sfile->achans > 0 && (!mainw->preview || (mainw->preview && mainw->is_processing)) &&
          (sfile->laudio_time > 0. || sfile->opening ||
           (mainw->multitrack && mainw->multitrack->is_rendering &&
            lives_file_test((tmpfilename = lives_get_audio_file_name(fileno)), LIVES_FILE_TEST_EXISTS)))) {
        lives_jack_set_client_attributes(jackd, fileno, TRUE, FALSE);
      }

      jack_audio_seek_bytes(jackd, sfile->aseek_pos, sfile);
      if (!await_audio_queue(LIVES_DEFAULT_TIMEOUT)) seek_err = TRUE;
    }

    if ((mainw->agen_key != 0 || mainw->agen_needs_reinit)
        && !mainw->multitrack && !mainw->preview) jackd->in_use = TRUE; // audio generator is active

    if (AUD_SRC_EXTERNAL && (prefs->audio_opts & AUDIO_OPTS_EXT_FX)) register_audio_client(FALSE);
    if (prefs->audio_opts & AUDIO_OPTS_AUX_PLAY) register_aux_audio_channels(1);

    mainw->rec_aclip = jackd->playing_file;
    if (mainw->rec_aclip != -1) {
      mainw->rec_aseek = fabs((double)fwd_seek_pos
                              / (double)(afile->achans * afile->asampsize / 8) / (double)afile->arps)
                         + (double)(mainw->startticks - mainw->currticks) / TICKS_PER_SECOND_DBL;
      mainw->rec_avel = fabs((double)jackd->sample_in_rate
                             / (double)afile->arps) * (double)afile->adirection;
    }
  }
}


static const char *xouts[JACK_MAX_PORTS];
static lives_pid_t iop_pid = 0;

#define IOPCLIENT "jamin"
//#define IOPCLIENT "zita-rev1"
//#define IOPCLIENT "Qtractor"
static boolean inter = FALSE;
static boolean need_clnup = FALSE;

static lives_proc_thread_t interop_lpt = NULL;

boolean jack_interop_cleanup(lives_obj_t *obj, void *data) {
  jack_driver_t *jackd = (jack_driver_t *)data;
  if (interop_lpt) {
    lives_hook_remove(interop_lpt);
    interop_lpt = NULL;
  }
  // reconnect
  int nch = jackd->num_output_channels;
  for (int i = 0; i < nch; i++) {
    if (xouts[i]) {
      jack_port_disconnect(jackd->client, jackd->output_port[i]);
      jack_connect(jackd->client, jack_port_name(jackd->output_port[i]), xouts[i]);
      xouts[i] = NULL;
    }
  }

  if (iop_pid) {
    lives_kill(iop_pid, LIVES_SIGKILL);
    iop_pid = 0;
  }

  if (need_clnup) {
    if (mainw->play_window) {
      lives_window_set_transient_for(LIVES_WINDOW(mainw->play_window), LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      gtk_window_set_skip_taskbar_hint(LIVES_WINDOW(mainw->play_window), TRUE);
      gtk_window_set_skip_pager_hint(LIVES_WINDOW(mainw->play_window), TRUE);
    }
    lives_widget_show(LIVES_MAIN_WINDOW_WIDGET);
    need_clnup = FALSE;
  }
  return FALSE;
}


boolean jack_interop_callback(LiVESAccelGroup * group, LiVESWidgetObject * obj, uint32_t keyval, LiVESXModifierType mod,
                              livespointer pjackd) {
  const char *iopclient = IOPCLIENT;
  const char **outs;

  if (pjackd) {
    size_t ioplen = lives_strlen(iopclient);
    jack_driver_t *jackd = (jack_driver_t *)pjackd;
    boolean launched = FALSE;
    int x, i, retries = 0;
    int nports = jackd->num_output_channels;

    if ((lives_get_status() != LIVES_STATUS_PLAYING && mainw->status != LIVES_STATUS_IDLE)
        || prefs->audio_player != AUD_PLAYER_JACK || AUD_SRC_EXTERNAL) return TRUE;

    if (!inter) {
      // disconnect from sys ports and reconnect
#ifdef GDK_WINDOWING_X11
      char *wid;
#endif
      const char **ports = jack_get_ports(jackd->client, NULL, NULL, jackd->jack_port_flags);
      boolean found = FALSE;
      if (!ports) return FALSE;

      for (i = 0; i < JACK_MAX_PORTS; i++) xouts[i] = NULL;

      for (x = 0; x < nports; x++) {
        outs = jack_port_get_connections(jackd->output_port[x]);
        if (outs) {
          jack_port_disconnect(jackd->client, jackd->output_port[x]);
          xouts[x] = outs[0];
          lives_free(outs);
        }
      }

retry:
      x = 0;
      for (i = 0; ports[i]; i++) {
        if (!lives_strncmp(ports[i], iopclient, ioplen)) {
          found = TRUE;
          // TODO - timeout
          if (jack_connect(jackd->client, jack_port_name(jackd->output_port[x++]), ports[i])) {
            // connection failed
            break;
          }
          if (x == nports) break;
        }
      }

      if (!found) {
        if (!launched) {
          char *com = lives_strdup_printf("%s", IOPCLIENT);
          iop_pid = lives_fork(com);
          lives_free(com);
          if (!iop_pid) {
            jack_interop_cleanup(NULL, jackd);
            return FALSE;
          }
          retries = MAX_CONX_TRIES;
          launched = TRUE;
        }
        if (retries-- > 0) {
          int pidchk = lives_kill(iop_pid, 0);
          if (!pidchk) {
            lives_nanosleep(LIVES_WAIT_A_SEC);
            goto retry;
          }
        }
        jack_interop_cleanup(NULL, jackd);
        return FALSE;
      }

#ifdef GDK_WINDOWING_X11
      wid = get_wid_for_name(IOPCLIENT);
      if (wid) activate_x11_window(wid);
      lives_free(wid);
#endif
      pref_factory_bool(PREF_SEPWIN, TRUE, FALSE);
      gtk_window_set_skip_taskbar_hint(LIVES_WINDOW(mainw->play_window), FALSE);
      gtk_window_set_skip_pager_hint(LIVES_WINDOW(mainw->play_window), FALSE);
      lives_window_set_transient_for(LIVES_WINDOW(mainw->play_window), NULL);
      lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);
#ifdef GDK_WINDOWING_X11
      if (capable->has_xdotool != MISSING || capable->has_wmctrl != MISSING) {
        wid = lives_strdup_printf("0x%08lx",
                                  (uint64_t)LIVES_XWINDOW_XID(lives_widget_get_xwindow(mainw->play_window)));
        if (wid) {
          activate_x11_window(wid);
          lives_free(wid);
        }
      }
#endif
      interop_lpt =
        lives_hook_append(NULL, COMPLETED_HOOK, HOOK_OPT_ONESHOT | HOOK_UNIQUE_FUNC, jack_interop_cleanup, jackd);
      need_clnup = TRUE;
    } else {
      jack_interop_cleanup(NULL, jackd);
    }
    inter = !inter;
  }
  return FALSE;
}

#undef afile

#endif
