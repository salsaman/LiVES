// omc-learn.c
// LiVES (lives-exe)
// (c) G. Finch 2008 - 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifdef ENABLE_OSC

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#endif

#include "main.h"
#include "paramwindow.h"
#include "effects.h"
#include "interface.h"

#include "omc-learn.h"

#ifdef OMC_JS_IMPL
#include <linux/joystick.h>
#endif

#include <errno.h>

// learn and match with an external control
// generally, external data is passed in as a type and a string (a sequence ascii encoded ints separated by spaces)
// the string will have a fixed sig(nature) which is matched against learned nodes
//
// the number of fixed values depends on the origin of the data; for example for a MIDI controller it is 2 (controller + controller number)
// the rest of the string is variables. These are either mapped in order to the parameters of the macro or can be filtered against

// these types/strings are matched against OMC macros - the macros have slots for parameters which are filled in order from variables in the input

// TODO !! - greedy matching should done - i.e. if an input sequence matches more than one macro, each of those macros will be triggered
// for now, only first match is acted on


// some events are filtered out, for example MIDI_NOTE_OFF, joystick button release; this needs to be done automatically

// TODO: we need end up with a table (struct *) like:
// int supertype;
// int ntypes;
// int *nfixed;
// int **min;
// int **max;
// boolean *uses_index;
// char **ignore;

// where min/max are not known we will need to calibrate


static OSCbuf obuf;
static char byarr[OSC_BUF_SIZE];
static lives_omc_macro_t omc_macros[N_OMC_MACROS];
static LiVESSList *omc_node_list;
static boolean omc_macros_inited=FALSE;

//////////////////////////////////////////////////////////////


static void omc_match_node_free(lives_omc_match_node_t *mnode) {

  if (mnode->nvars>0) {
    lives_free(mnode->offs0);
    lives_free(mnode->scale);
    lives_free(mnode->offs1);
    lives_free(mnode->min);
    lives_free(mnode->max);
    lives_free(mnode->matchp);
    lives_free(mnode->matchi);
  }

  if (mnode->map!=NULL) lives_free(mnode->map);
  if (mnode->fvali!=NULL) lives_free(mnode->fvali);
  if (mnode->fvald!=NULL) lives_free(mnode->fvald);

  lives_free(mnode->srch);

  lives_free(mnode);

}



static void remove_all_nodes(boolean every, omclearn_w *omclw) {
  lives_omc_match_node_t *mnode;
  LiVESSList *slist_last=NULL,*slist_next;
  LiVESSList *slist=omc_node_list;

  while (slist!=NULL) {
    slist_next=slist->next;

    mnode=(lives_omc_match_node_t *)slist->data;

    if (every||mnode->macro==-1) {
      if (slist_last!=NULL) slist_last->next=slist->next;
      else omc_node_list=slist->next;
      omc_match_node_free(mnode);
    } else slist_last=slist;
    slist=slist_next;
  }

  lives_widget_set_sensitive(omclw->clear_button,FALSE);
  if (slist==NULL) lives_widget_set_sensitive(omclw->del_all_button,FALSE);

}


static LIVES_INLINE int js_index(const char *string) {
  // js index, or midi channel number
  char **array=lives_strsplit(string," ",-1);
  int res=atoi(array[1]);
  lives_strfreev(array);
  return res;
}


static LIVES_INLINE int midi_index(const char *string) {
  // midi controller number
  char **array;
  int res;
  if (get_token_count(string,' ')<3) return -1;

  array=lives_strsplit(string," ",-1);
  res=atoi(array[2]);
  lives_strfreev(array);
  return res;
}


#ifdef OMC_JS_IMPL



static int js_fd;



#ifndef IS_MINGW
const char *get_js_filename(void) {
  char *js_fname;

  // OPEN DEVICE FILE
  // first try to open /dev/input/js
  js_fname = "/dev/input/js";
  js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
  if (js_fd < 0) {
    // if it doesn't open, try to open /dev/input/js0
    js_fname = "/dev/input/js0";
    js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
    if (js_fd < 0) {
      js_fname = "/dev/js0";
      js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
      // if no device is found
      if (js_fd < 0) {
        return NULL;
      }
    }
  }
  return js_fname;
}
#endif


boolean js_open(void) {

  if (!(prefs->omc_dev_opts&OMC_DEV_JS)) return TRUE;

  if (prefs->omc_js_fname!=NULL) {
    js_fd = open(prefs->omc_js_fname, O_RDONLY|O_NONBLOCK);
    if (js_fd < 0) return FALSE;
  } else {
    const char *tmp=get_js_filename();
    if (tmp!=NULL) {
      lives_snprintf(prefs->omc_js_fname,256,"%s",tmp);
    }
  }
  if (prefs->omc_js_fname==NULL) return FALSE;

  mainw->ext_cntl[EXT_CNTL_JS]=TRUE;
  d_print(_("Responding to joystick events from %s\n"),prefs->omc_js_fname);

  return TRUE;
}



void js_close(void) {
  if (mainw->ext_cntl[EXT_CNTL_JS]) {
    close(js_fd);
    mainw->ext_cntl[EXT_CNTL_JS]=FALSE;
  }
}


char *js_mangle(void) {
  // get js event and process it
  struct js_event jse;
  size_t bytes;
  char *ret;
  int type=0;

  bytes = read(js_fd, &jse, sizeof(jse));

  if (bytes!=sizeof(jse)) return NULL;

  jse.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
  if (jse.type == JS_EVENT_AXIS) {
    type=OMC_JS_AXIS;
    if (jse.value==0) return NULL;
  } else if (jse.type == JS_EVENT_BUTTON) {
    if (jse.value==0) return NULL;
    type=OMC_JS_BUTTON;
  }

  ret=lives_strdup_printf("%d %d %d",type,jse.number,jse.value);

  return ret;

}


static LIVES_INLINE int js_msg_type(const char *string) {
  return atoi(string);
}



#endif  // OMC_JS


#ifdef OMC_MIDI_IMPL

static int midi_fd;

#ifndef IS_MINGW


const char *get_midi_filename(void) {
  char *midi_fname;

  // OPEN DEVICE FILE
  midi_fname = "/dev/midi";
  midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
  if (midi_fd < 0) {
    midi_fname = "/dev/midi0";
    midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
    if (midi_fd < 0) {
      midi_fname = "/dev/midi1";
      midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
      if (midi_fd < 0) {
        return NULL;
      }
    }
  }
  return midi_fname;
}

#endif


boolean midi_open(void) {

  if (!(prefs->omc_dev_opts&OMC_DEV_MIDI)) return TRUE;

#ifdef ALSA_MIDI
  if (prefs->use_alsa_midi) {

    d_print(_("Creating ALSA seq port..."));

    // ORL Ouverture d'un port ALSA
    if (snd_seq_open(&mainw->seq_handle, "default", SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK) < 0) {
      d_print_failed();
      return FALSE;
    }

    snd_seq_set_client_name(mainw->seq_handle, "LiVES");
    if ((mainw->alsa_midi_port = snd_seq_create_simple_port(mainw->seq_handle, "LiVES",
                                 SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
                                 SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_PORT|SND_SEQ_PORT_TYPE_SOFTWARE)) < 0) {

      d_print_failed();
      return FALSE;
    }

    d_print_done();
  } else {

#endif

#ifndef IS_MINGW
    if (prefs->omc_midi_fname!=NULL) {
      midi_fd = open(prefs->omc_midi_fname, O_RDONLY|O_NONBLOCK);
      if (midi_fd < 0) return FALSE;
    } else {
      const char *tmp=get_midi_filename();
      if (tmp!=NULL) {
        lives_snprintf(prefs->omc_midi_fname,256,"%s",tmp);
      }
    }
    if (prefs->omc_midi_fname==NULL) return FALSE;

    d_print(_("Responding to MIDI events from %s\n"),prefs->omc_midi_fname);
#endif

#ifdef ALSA_MIDI
  }
#endif

  mainw->ext_cntl[EXT_CNTL_MIDI]=TRUE;

  return TRUE;
}



void midi_close(void) {
  if (mainw->ext_cntl[EXT_CNTL_MIDI]) {

#ifdef ALSA_MIDI
    if (mainw->seq_handle!=NULL) {
      // close
      snd_seq_delete_simple_port(mainw->seq_handle,mainw->alsa_midi_port);
      snd_seq_close(mainw->seq_handle);
      mainw->seq_handle=NULL;
    } else {

#endif

      close(midi_fd);

#ifdef ALSA_MIDI
    }
#endif

    mainw->ext_cntl[EXT_CNTL_MIDI]=FALSE;
  }
}



static int get_midi_len(int msgtype) {
  switch (msgtype) {
  case OMC_MIDI_CONTROLLER:
  case OMC_MIDI_NOTE:
  case OMC_MIDI_PITCH_BEND:
    return 3;
  case OMC_MIDI_NOTE_OFF:
  case OMC_MIDI_PGM_CHANGE:
    return 2;
  }
  return 0;
}


static int midi_msg_type(const char *string) {
  int type=atoi(string);

  if ((type&0XF0)==0X90) return OMC_MIDI_NOTE;
  if ((type&0XF0)==0x80) return OMC_MIDI_NOTE_OFF;
  if ((type&0XF0)==0xB0) return OMC_MIDI_CONTROLLER;
  if ((type&0XF0)==0xC0) return OMC_MIDI_PGM_CHANGE;
  if ((type&0XF0)==0xE0) return OMC_MIDI_PITCH_BEND;


  // other types are currently ignored
  // 0XA0 is aftertouch, has key and value

  // OxC0 is patch change, with 2 bytes parm
  // 0xD0 is channel pressure, 1 byte parm

  // 0xE0 is pitch bend: lsb 7 bits, msb 7 bits

  // 0XF0 is sysex

  return 0;
}



char *midi_mangle(void) {
  // get MIDI event and process it
  char *string=NULL;

  ssize_t bytes,tot=0,allowed=prefs->midi_rpt;
  unsigned char midbuf[4],xbuf[4];
  int target=1,mtype=0,idx;
  boolean got_target=FALSE;
  char *str;

#ifdef ALSA_MIDI
  int npfd=0;
  struct pollfd *pfd=NULL;
  snd_seq_event_t *ev;
  int typeNumber;
  boolean hasmore=FALSE;

  if (mainw->seq_handle!=NULL) {

    if (snd_seq_event_input_pending(mainw->seq_handle, 0)==0) {
      // returns number of poll descriptors
      npfd = snd_seq_poll_descriptors_count(mainw->seq_handle, POLLIN);

      if (npfd<1) return NULL;

      pfd = (struct pollfd *)lives_malloc(npfd * sizeof(struct pollfd));

      // fill our poll descriptors
      snd_seq_poll_descriptors(mainw->seq_handle, pfd, npfd, POLLIN);
    } else hasmore=TRUE; // events remaining from the last call to this function

    if (hasmore || poll(pfd, npfd, 0) > 0) {

      do {

        if (snd_seq_event_input(mainw->seq_handle, &ev)<0) {
          break; // an error occured reading from the port
        }

        switch (ev->type) {
        case SND_SEQ_EVENT_CONTROLLER:
          typeNumber=176;
          string=lives_strdup_printf("%d %d %u %d",typeNumber+ev->data.control.channel, ev->data.control.channel, ev->data.control.param,
                                     ev->data.control.value);

          break;
        case SND_SEQ_EVENT_PITCHBEND:
          typeNumber=224;
          string=lives_strdup_printf("%d %d %d",typeNumber+ev->data.control.channel,ev->data.control.channel, ev->data.control.value);
          break;

        case SND_SEQ_EVENT_NOTEON:
          typeNumber=144;
          string=lives_strdup_printf("%d %d %d %d",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.note.note,
                                     ev->data.note.velocity);

          break;
        case SND_SEQ_EVENT_NOTEOFF:
          typeNumber=128;
          string=lives_strdup_printf("%d %d %d %d",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.note.note,
                                     ev->data.note.off_velocity);

          break;
        case SND_SEQ_EVENT_PGMCHANGE:
          typeNumber=192;
          string=lives_strdup_printf("%d %d %d",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.control.value);

          break;

        }
        snd_seq_free_event(ev);

      } while (snd_seq_event_input_pending(mainw->seq_handle, 0) > 0 && string==NULL);

    }

    if (pfd!=NULL) lives_free(pfd);

  } else {

#endif

    if (midi_fd==-1) return NULL;

    while (tot<target) {
      bytes = read(midi_fd, xbuf, target-tot);

      if (bytes<1) {
        if (--allowed<0) return NULL;
        continue;
      }

      str=lives_strdup_printf("%d",xbuf[0]);

      if (!got_target) {
        target=get_midi_len((mtype=midi_msg_type(str)));
        got_target=TRUE;
      }

      lives_free(str);

      //g_print("midi pip %d %02X , tg=%d\n",bytes,xbuf[0],target);

      memcpy(midbuf+tot,xbuf,bytes);

      tot+=bytes;

    }

    if (mtype==0) return NULL;

    idx=(midbuf[0]&0x0F);

    if (target==2) string=lives_strdup_printf("%u %u %u",midbuf[0],idx,midbuf[1]);
    else if (target==3) string=lives_strdup_printf("%u %u %u %u",midbuf[0],idx,midbuf[1],midbuf[2]);
    else string=lives_strdup_printf("%u %u %u %u %u",midbuf[0],idx,midbuf[1],midbuf[2],midbuf[3]);


#ifdef ALSA_MIDI
  }
#endif

  //g_print("got %s\n",string);

  return string;
}


#endif //OMC_MIDI_IMPL



static LIVES_INLINE char *cut_string_elems(const char *string, int nelems) {
  // remove elements after nelems

  char *retval=lives_strdup(string);
  register int i;
  size_t slen=strlen(string);

  if (nelems<0) return retval;

  for (i=0; i<slen; i++) {
    if (!strncmp((string+i)," ",1)) {
      if (--nelems==0) {
        memset(retval+i,0,1);
        return retval;
      }
    }
  }
  return retval;
}







static char *omc_learn_get_pname(int type, int idx) {
  switch (type) {
  case OMC_MIDI_CONTROLLER:
  case OMC_MIDI_PITCH_BEND:
  case OMC_MIDI_PGM_CHANGE:
    return lives_strdup(_("data"));
  case OMC_MIDI_NOTE:
  case OMC_MIDI_NOTE_OFF:
    if (idx==1) return lives_strdup(_("velocity"));
    return lives_strdup(_("note"));
  case OMC_JS_AXIS:
    return lives_strdup(_("value"));
  default:
    return lives_strdup(_("state"));
  }
}



static int omc_learn_get_pvalue(int type, int idx, const char *string) {
  char **array=lives_strsplit(string," ",-1);
  int res;

  switch (type) {
  case OMC_MIDI_CONTROLLER:
    res=atoi(array[3+idx]);
    break;
  default:
    res=atoi(array[2+idx]);
    break;
  }

  lives_strfreev(array);
  return res;
}



static void cell1_edited_callback(LiVESCellRenderer *spinbutton, const char *path_string, const char *new_text, livespointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;

  lives_omc_macro_t omacro=omc_macros[mnode->macro];

  int vali;
  double vald;

  LiVESTreeIter iter;

  int row;

  int *indices;

  LiVESTreePath *tpath=lives_tree_path_new_from_string(path_string);

  if (lives_tree_path_get_depth(tpath)!=2) {
    lives_tree_path_free(tpath);
    return;
  }

  indices=lives_tree_path_get_indices(tpath);
  row=indices[1];

  lives_tree_model_get_iter(LIVES_TREE_MODEL(mnode->gtkstore2),&iter,tpath);

  lives_tree_path_free(tpath);

  if (row>(omacro.nparams-mnode->nvars)) {
    // text, so dont alter
    return;
  }

  switch (omacro.ptypes[row]) {
  case OMC_PARAM_INT:
    vali=atoi(new_text);
    mnode->fvali[row]=vali;
    break;
  case OMC_PARAM_DOUBLE:
    vald=lives_strtod(new_text,NULL);
    mnode->fvald[row]=vald;
    break;
  }

  lives_tree_store_set(mnode->gtkstore2,&iter,VALUE2_COLUMN,new_text,-1);

}


static void rowexpand(LiVESWidget *tv, LiVESTreeIter *iter, LiVESTreePath *path, livespointer ud) {
  lives_widget_queue_resize(tv);
}


static void omc_macro_row_add_params(lives_omc_match_node_t *mnode, int row, omclearn_w *omclw) {
  lives_omc_macro_t macro=omc_macros[mnode->macro];

  LiVESCellRenderer *renderer;
  LiVESTreeViewColumn *column;

  LiVESTreeIter iter1,iter2;

  LiVESObject *spinadj;

  char *strval=NULL,*vname;
  char *oldval=NULL,*final=NULL;

  int mfrom;
  register int i;


  mnode->gtkstore2 = lives_tree_store_new(NUM2_COLUMNS, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING);

  if (macro.nparams==0) return;

  lives_tree_store_append(mnode->gtkstore2, &iter1, NULL);   /* Acquire an iterator */
  lives_tree_store_set(mnode->gtkstore2, &iter1, TITLE2_COLUMN, (_("Params.")), -1);

  for (i=0; i<macro.nparams; i++) {
    lives_tree_store_append(mnode->gtkstore2, &iter2, &iter1);   /* Acquire a child iterator */

    if (oldval!=NULL) {
      lives_free(oldval);
      oldval=NULL;
    }

    if (final!=NULL) {
      lives_free(final);
      final=NULL;
    }

    if ((mfrom=mnode->map[i])!=-1) strval=lives_strdup(_("variable"));
    else {
      switch (macro.ptypes[i]) {
      case OMC_PARAM_INT:
        strval=lives_strdup_printf("%d",mnode->fvali[i]);
        break;
      case OMC_PARAM_DOUBLE:
        strval=lives_strdup_printf("%.*f",OMC_FP_FIX,mnode->fvald[i]);
        break;

      }
    }

    vname=macro.pname[i];

    lives_tree_store_set(mnode->gtkstore2, &iter2, TITLE2_COLUMN, vname, VALUE2_COLUMN, strval, -1);
  }

  lives_free(strval);

  mnode->treev2 = lives_tree_view_new_with_model(LIVES_TREE_MODEL(mnode->gtkstore2));

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(mnode->treev2, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_text_color(mnode->treev2, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(NULL,
           renderer,
           "text", TITLE2_COLUMN,
           NULL);

  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev2), column);

  renderer = lives_cell_renderer_spin_new();

  spinadj=(LiVESObject *)lives_adjustment_new(0., -100000., 100000., 1., 10., 0);

#ifdef GUI_GTK
  g_object_set(renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
               "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);

#endif

  lives_signal_connect(renderer, LIVES_WIDGET_EDITED_SIGNAL, LIVES_GUI_CALLBACK(cell1_edited_callback), mnode);



  //  renderer = lives_cell_renderer_text_new ();
  column = lives_tree_view_column_new_with_attributes(_("value"),
           renderer,
           "text", VALUE2_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev2), column);


  lives_widget_show(mnode->treev2);

  lives_signal_connect(LIVES_GUI_OBJECT(mnode->treev2), LIVES_WIDGET_ROW_EXPANDED_SIGNAL,
                       LIVES_GUI_CALLBACK(rowexpand),
                       NULL);

  lives_table_attach(LIVES_TABLE(omclw->table), mnode->treev2, 3, 4, row, row+1,
                     (LiVESAttachOptions)(LIVES_FILL|LIVES_EXPAND),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);



}




static void omc_learn_link_params(lives_omc_match_node_t *mnode) {
  lives_omc_macro_t omc_macro=omc_macros[mnode->macro];
  int mps=omc_macro.nparams-1;
  int lps=mnode->nvars-1;
  int i;

  if (mnode->map!=NULL) lives_free(mnode->map);
  if (mnode->fvali!=NULL) lives_free(mnode->fvali);
  if (mnode->fvald!=NULL) lives_free(mnode->fvald);

  mnode->map=(int *)lives_malloc(omc_macro.nparams*sizint);
  mnode->fvali=(int *)lives_malloc(omc_macro.nparams*sizint);
  mnode->fvald=(double *)lives_malloc(omc_macro.nparams*sizdbl);

  if (lps>mps) lps=mps;

  for (i=mps; i>=0; i--) {
    if (mnode->matchp[lps]) lps++; // variable is filtered for
  }

  for (i=mps; i>=0; i--) {
    if (lps<0||lps>=mnode->nvars) {
      //g_print("fixed !\n");
      mnode->map[i]=-1;
      if (omc_macro.ptypes[i]==OMC_PARAM_INT) mnode->fvali[i]=omc_macro.vali[i];
      else mnode->fvald[i]=omc_macro.vald[i];
    } else {
      //      g_print("varied !\n");
      if (!mnode->matchp[lps]) mnode->map[i]=lps;
      else i++;
    }
    lps--;
  }

}









static void on_omc_combo_entry_changed(LiVESCombo *combo, livespointer ptr) {
  char *macro_text;

  int i;

  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)ptr;

  int row=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo),"row"));
  omclearn_w *omclw=(omclearn_w *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(combo),"omclw");

  macro_text=lives_combo_get_active_text(LIVES_COMBO(combo));

  if (mnode->treev2!=NULL) {
    // remove old mapping
    lives_widget_destroy(mnode->treev2);
    mnode->treev2=NULL;

    mnode->macro=-1;

    lives_free(mnode->map);
    lives_free(mnode->fvali);
    lives_free(mnode->fvald);

    mnode->map=mnode->fvali=NULL;
    mnode->fvald=NULL;

  }

  if (!strcmp(macro_text,mainw->string_constants[LIVES_STRING_CONSTANT_NONE])) {
    lives_free(macro_text);
    return;
  }

  for (i=0; i<=N_OMC_MACROS; i++) {
    if (!strcmp(macro_text,omc_macros[i].macro_text)) break;
  }

  lives_free(macro_text);

  mnode->macro=i;
  omc_learn_link_params(mnode);
  omc_macro_row_add_params(mnode,row,omclw);

}




static void cell_toggled_callback(LiVESCellRenderer *toggle, const char *path_string, livespointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;
  int row;

  char *txt;

  int *indices;

  LiVESTreePath *tpath=lives_tree_path_new_from_string(path_string);

  LiVESTreeIter iter;

  if (lives_tree_path_get_depth(tpath)!=2) {
    lives_tree_path_free(tpath);
    return;
  }

  indices=lives_tree_path_get_indices(tpath);
  row=indices[1];

  lives_tree_model_get_iter(LIVES_TREE_MODEL(mnode->gtkstore),&iter,tpath);

  lives_tree_path_free(tpath);

  lives_tree_model_get(LIVES_TREE_MODEL(mnode->gtkstore),&iter,VALUE_COLUMN,&txt,-1);

  if (!strcmp(txt,"-")) {
    lives_free(txt);
    return;
  }

  lives_free(txt);

  mnode->matchp[row]=!(mnode->matchp[row]);

  lives_tree_store_set(mnode->gtkstore,&iter,FILTER_COLUMN,mnode->matchp[row],-1);

  omc_learn_link_params(mnode);

}



static void cell_edited_callback(LiVESCellRenderer *spinbutton, const char *path_string, const char *new_text, livespointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;

  int col=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(spinbutton),"colnum"));

  int vali;
  double vald;

  LiVESTreeIter iter;

  int row;

  int *indices;

  LiVESTreePath *tpath=lives_tree_path_new_from_string(path_string);

  if (lives_tree_path_get_depth(tpath)!=2) {
    lives_tree_path_free(tpath);
    return;
  }

  indices=lives_tree_path_get_indices(tpath);
  row=indices[1];

  lives_tree_model_get_iter(LIVES_TREE_MODEL(mnode->gtkstore),&iter,tpath);

  lives_tree_path_free(tpath);

  switch (col) {
  case OFFS1_COLUMN:
    vali=atoi(new_text);
    mnode->offs0[row]=vali;
    break;
  case OFFS2_COLUMN:
    vali=atoi(new_text);
    mnode->offs1[row]=vali;
    break;
  case SCALE_COLUMN:
    vald=lives_strtod(new_text,NULL);
    mnode->scale[row]=vald;
    break;
  }

  lives_tree_store_set(mnode->gtkstore,&iter,col,new_text,-1);

}








static LiVESWidget *create_omc_macro_combo(lives_omc_match_node_t *mnode, int row, omclearn_w *omclw) {
  int i;

  LiVESWidget *combo;

  combo=lives_combo_new();

  for (i=0; i<N_OMC_MACROS; i++) {
    if (omc_macros[i].msg==NULL) break;

    lives_combo_append_text(LIVES_COMBO(combo),omc_macros[i].macro_text);

  }

  if (mnode->macro!=-1) {
    lives_combo_set_active_index(LIVES_COMBO(combo),mnode->macro);
  }

  lives_signal_connect_after(LIVES_WIDGET_OBJECT(combo), LIVES_WIDGET_CHANGED_SIGNAL, LIVES_GUI_CALLBACK(on_omc_combo_entry_changed), mnode);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo),"row",LIVES_INT_TO_POINTER(row));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(combo),"omclw",(livespointer)omclw);

  return combo;
}



static void omc_learner_add_row(int type, int detail, lives_omc_match_node_t *mnode, const char *string, omclearn_w *omclw) {
  LiVESWidget *label,*combo;
  LiVESObject *spinadj;

  LiVESCellRenderer *renderer;
  LiVESTreeViewColumn *column;

  LiVESTreeIter iter1,iter2;

  char *strval,*strval2,*strval3,*strval4,*vname,*valstr;
  char *oldval=NULL,*final=NULL;
  char *labelt=NULL;

  int chan,val;
  register int i;

  omclw->tbl_rows++;
  lives_table_resize(LIVES_TABLE(omclw->table),omclw->tbl_rows,4);

  mnode->gtkstore = lives_tree_store_new(NUM_COLUMNS, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_BOOLEAN,
                                         LIVES_COL_TYPE_STRING,
                                         LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING, LIVES_COL_TYPE_STRING);

  lives_tree_store_append(mnode->gtkstore, &iter1, NULL);   /* Acquire an iterator */
  lives_tree_store_set(mnode->gtkstore, &iter1, TITLE_COLUMN, (_("Vars.")), -1);

  for (i=0; i<mnode->nvars; i++) {
    lives_tree_store_append(mnode->gtkstore, &iter2, &iter1);   /* Acquire a child iterator */

    if (oldval!=NULL) {
      lives_free(oldval);
      oldval=NULL;
    }

    if (final!=NULL) {
      lives_free(final);
      final=NULL;
    }

    strval=lives_strdup_printf("%d - %d",mnode->min[i],mnode->max[i]);
    strval2=lives_strdup_printf("%d",mnode->offs0[i]);
    strval3=lives_strdup_printf("%.*f",OMC_FP_FIX,mnode->scale[i]);
    strval4=lives_strdup_printf("%d",mnode->offs1[i]);

    if (type>0) {
      vname=omc_learn_get_pname(type,i);
      val=omc_learn_get_pvalue(type,i,string);

      valstr=lives_strdup_printf("%d",val);
      if (!mnode->matchp[i]) {
        mnode->matchi[i]=val;
      }
    } else {
      vname=omc_learn_get_pname(-type,i);
      if (mnode->matchp[i]) valstr=lives_strdup_printf("%d",mnode->matchi[i]);
      else valstr=lives_strdup("-");
    }

    lives_tree_store_set(mnode->gtkstore, &iter2, TITLE_COLUMN, vname, VALUE_COLUMN, valstr, FILTER_COLUMN, mnode->matchp[i],
                         RANGE_COLUMN, strval, OFFS1_COLUMN, strval2, SCALE_COLUMN, strval3, OFFS2_COLUMN, strval4, -1);

    lives_free(strval);
    lives_free(strval2);
    lives_free(strval3);
    lives_free(strval4);
    lives_free(valstr);
    lives_free(vname);
  }

  mnode->treev1 = lives_tree_view_new_with_model(LIVES_TREE_MODEL(mnode->gtkstore));

  if (type<0) type=-type;

  switch (type) {
  case OMC_MIDI_NOTE:
    chan=js_index(string);
    labelt=lives_strdup_printf(_("MIDI ch %d note on"),chan);
    break;
  case OMC_MIDI_NOTE_OFF:
    chan=js_index(string);
    labelt=lives_strdup_printf(_("MIDI ch %d note off"),chan);
    break;
  case OMC_MIDI_CONTROLLER:
    chan=js_index(string);
    labelt=lives_strdup_printf(_("MIDI ch %d controller %d"),chan,detail);
    break;
  case OMC_MIDI_PITCH_BEND:
    chan=js_index(string);
    labelt=lives_strdup_printf(_("MIDI ch %d pitch bend"),chan,detail);
    break;
  case OMC_MIDI_PGM_CHANGE:
    chan=js_index(string);
    labelt=lives_strdup_printf(_("MIDI ch %d pgm change"),chan);
    break;
  case OMC_JS_BUTTON:
    labelt=lives_strdup_printf(_("Joystick button %d"),detail);
    break;
  case OMC_JS_AXIS:
    labelt=lives_strdup_printf(_("Joystick axis %d"),detail);
    break;
  }

  label = lives_standard_label_new(labelt);
  lives_widget_show(label);

  if (labelt!=NULL) lives_free(labelt);


  omclw->tbl_currow++;

  lives_table_attach(LIVES_TABLE(omclw->table), label, 0, 1, omclw->tbl_currow, omclw->tbl_currow+1,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(0), 0, 0);

  // properties
  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(mnode->treev1, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_text_color(mnode->treev1, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(NULL,
           renderer,
           "text", TITLE_COLUMN,
           NULL);

  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(_("value"),
           renderer,
           "text", VALUE_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);


  renderer = lives_cell_renderer_toggle_new();
  column = lives_tree_view_column_new_with_attributes(_("x"),
           renderer,
           "active", FILTER_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);

  lives_signal_connect(renderer, LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(cell_toggled_callback), mnode);

  renderer = lives_cell_renderer_text_new();
  column = lives_tree_view_column_new_with_attributes(_("range"),
           renderer,
           "text", RANGE_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);


  renderer = lives_cell_renderer_spin_new();
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(renderer), "colnum", LIVES_UINT_TO_POINTER(OFFS1_COLUMN));

  spinadj=(LiVESObject *)lives_adjustment_new(0., -100000., 100000., 1., 10., 0);

#ifdef GUI_GTK
  g_object_set(renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
               "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);
#endif

  lives_signal_connect(renderer, LIVES_WIDGET_EDITED_SIGNAL, LIVES_GUI_CALLBACK(cell_edited_callback), mnode);



  column = lives_tree_view_column_new_with_attributes(_("+ offset1"),
           renderer,
           "text", OFFS1_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);

  renderer = lives_cell_renderer_spin_new();

  spinadj=(LiVESObject *)lives_adjustment_new(1., -100000., 100000., 1., 10., 0);

#ifdef GUI_GTK
  g_object_set(renderer, "width-chars", 12, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
               "editable", TRUE, "xalign", 1.0, "adjustment", spinadj,
               "digits", OMC_FP_FIX, NULL);
#endif

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(renderer), "colnum", LIVES_UINT_TO_POINTER(SCALE_COLUMN));
  lives_signal_connect(renderer, LIVES_WIDGET_EDITED_SIGNAL, LIVES_GUI_CALLBACK(cell_edited_callback), mnode);


  column = lives_tree_view_column_new_with_attributes(_("* scale"),
           renderer,
           "text", SCALE_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);

  renderer = lives_cell_renderer_spin_new();


  spinadj=(LiVESObject *)lives_adjustment_new(0., -100000., 100000., 1., 10., 0);

#ifdef GUI_GTK
  g_object_set(renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
               "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);
#endif

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(renderer), "colnum", LIVES_UINT_TO_POINTER(OFFS2_COLUMN));
  lives_signal_connect(renderer, LIVES_WIDGET_EDITED_SIGNAL, LIVES_GUI_CALLBACK(cell_edited_callback), mnode);


  column = lives_tree_view_column_new_with_attributes(_("+ offset2"),
           renderer,
           "text", OFFS2_COLUMN,
           NULL);
  lives_tree_view_append_column(LIVES_TREE_VIEW(mnode->treev1), column);

  lives_widget_show(mnode->treev1);

  lives_widget_set_size_request(mnode->treev1,-1,TREE_ROW_HEIGHT);

  lives_table_attach(LIVES_TABLE(omclw->table), mnode->treev1, 1, 2, omclw->tbl_currow, omclw->tbl_currow+1,
                     (LiVESAttachOptions)(0),
                     (LiVESAttachOptions)(LIVES_EXPAND), 0, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(mnode->treev1), LIVES_WIDGET_ROW_EXPANDED_SIGNAL,
                       LIVES_GUI_CALLBACK(rowexpand),
                       NULL);

  combo=create_omc_macro_combo(mnode,omclw->tbl_currow,omclw);

  lives_widget_show(combo);

  lives_table_attach(LIVES_TABLE(omclw->table), combo, 2, 3, omclw->tbl_currow, omclw->tbl_currow+1,
                     (LiVESAttachOptions) 0,
                     (LiVESAttachOptions)(0), 0, 0);


  if (mnode->macro==-1) lives_widget_set_sensitive(omclw->clear_button,TRUE);
  lives_widget_set_sensitive(omclw->del_all_button,TRUE);

}





static void killit(LiVESWidget *widget, livespointer user_data) {
  lives_widget_destroy(widget);
}


static void show_existing(omclearn_w *omclw) {
  LiVESSList *slist=omc_node_list;
  lives_omc_match_node_t *mnode;
  int type,supertype;
  char **array,*srch;
  int idx;

  while (slist!=NULL) {
    mnode=(lives_omc_match_node_t *)slist->data;

    srch=lives_strdup(mnode->srch);
    array=lives_strsplit(srch," ",-1);

    supertype=atoi(array[0]);
#ifdef OMC_MIDI_IMPL
    if (supertype==OMC_MIDI) {
      size_t blen;
      char *tmp;

      type=midi_msg_type(array[1]);
      if (get_token_count(srch,' ')>3) idx=atoi(array[3]);
      else idx=-1;
      srch=lives_strdup(mnode->srch);
      tmp=cut_string_elems(srch,1);
      blen=strlen(tmp);
      tmp=lives_strdup(srch+blen+1);
      lives_free(srch);
      srch=tmp;
    } else {
#endif
      type=supertype;
      idx=atoi(array[1]);
#ifdef OMC_MIDI_IMPL
    }
#endif
    lives_strfreev(array);

    omc_learner_add_row(-type,idx,mnode,srch,omclw);
    lives_free(srch);

    omc_macro_row_add_params(mnode,omclw->tbl_currow,omclw);

    slist=slist->next;
  }
}



static void clear_unmatched(LiVESButton *button, livespointer user_data) {
  omclearn_w *omclw=(omclearn_w *)user_data;

  // destroy everything in table

  lives_container_foreach(LIVES_CONTAINER(omclw->table),killit,NULL);

  omclw->tbl_currow=-1;

  remove_all_nodes(FALSE,omclw);

  show_existing(omclw);

}


static void del_all(LiVESButton *button, livespointer user_data) {
  omclearn_w *omclw=(omclearn_w *)user_data;

  if (!do_warning_dialog(_("\nClick OK to delete all entries\n"))) return;

  // destroy everything in table

  lives_container_foreach(LIVES_CONTAINER(omclw->table),killit,NULL);

  remove_all_nodes(TRUE,omclw);

}



static void close_learner_dialog(LiVESButton *button, livespointer user_data) {
  mainw->cancelled=CANCEL_USER;
}



static omclearn_w *create_omclearn_dialog(void) {
  LiVESWidget *ok_button;
  LiVESWidget *hbuttonbox;
  LiVESWidget *scrolledwindow;
  int winsize_h,scr_width=mainw->scr_width;
  int winsize_v,scr_height=mainw->scr_height;

  omclearn_w *omclw=(omclearn_w *)lives_malloc(sizeof(omclearn_w));

  omclw->tbl_rows=4;
  omclw->tbl_currow=-1;

  if (prefs->gui_monitor!=0) {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  winsize_h=scr_width-SCR_WIDTH_SAFETY;
  winsize_v=scr_height-SCR_HEIGHT_SAFETY;

  omclw->dialog = lives_standard_dialog_new(_("LiVES: OMC learner"),FALSE,winsize_h,winsize_v);

  omclw->top_vbox = lives_dialog_get_content_area(LIVES_DIALOG(omclw->dialog));

  omclw->table = lives_table_new(omclw->tbl_rows, 4, FALSE);

  lives_table_set_col_spacings(LIVES_TABLE(omclw->table),widget_opts.packing_width*2);

  scrolledwindow = lives_standard_scrolled_window_new(winsize_h, winsize_v-SCR_HEIGHT_SAFETY, omclw->table);

  lives_box_pack_start(LIVES_BOX(omclw->top_vbox), scrolledwindow, TRUE, TRUE, 0);



  hbuttonbox = lives_dialog_get_action_area(LIVES_DIALOG(omclw->dialog));

  omclw->clear_button = lives_button_new_from_stock(LIVES_STOCK_CLEAR,_("Clear _unmatched"));

  lives_container_add(LIVES_CONTAINER(hbuttonbox), omclw->clear_button);


  lives_signal_connect(LIVES_GUI_OBJECT(omclw->clear_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(clear_unmatched),
                       (livespointer)omclw);

  lives_widget_set_sensitive(omclw->clear_button,FALSE);

  omclw->del_all_button = lives_button_new_from_stock(LIVES_STOCK_DELETE,_("_Delete all"));

  lives_container_add(LIVES_CONTAINER(hbuttonbox), omclw->del_all_button);


  lives_signal_connect(LIVES_GUI_OBJECT(omclw->del_all_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(del_all),
                       (livespointer)omclw);

  lives_widget_set_sensitive(omclw->del_all_button,FALSE);


  ok_button = lives_button_new_from_stock(LIVES_STOCK_CLOSE,_("_Close Window"));

  lives_container_add(LIVES_CONTAINER(hbuttonbox), ok_button);

  lives_widget_set_can_focus_and_default(ok_button);

  lives_widget_grab_default(ok_button);

  lives_button_box_set_button_width(LIVES_BUTTON_BOX(hbuttonbox), ok_button, DEF_BUTTON_WIDTH*4);
  lives_button_box_set_button_width(LIVES_BUTTON_BOX(hbuttonbox), omclw->clear_button, DEF_BUTTON_WIDTH*4);
  lives_button_box_set_button_width(LIVES_BUTTON_BOX(hbuttonbox), omclw->del_all_button, DEF_BUTTON_WIDTH*4);

  lives_button_box_set_layout(LIVES_BUTTON_BOX(hbuttonbox), LIVES_BUTTONBOX_SPREAD);


  lives_signal_connect(LIVES_GUI_OBJECT(ok_button), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(close_learner_dialog),
                       NULL);

  if (prefs->gui_monitor!=0) {
    int xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width
             -lives_widget_get_allocation_width(omclw->dialog))/2;
    int ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height
             -lives_widget_get_allocation_height(omclw->dialog))/2;
    lives_window_set_screen(LIVES_WINDOW(omclw->dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
    lives_window_move(LIVES_WINDOW(omclw->dialog),xcen,ycen);
  }

  if (prefs->open_maximised) {
    lives_window_maximize(LIVES_WINDOW(omclw->dialog));
  }

  if (prefs->show_gui)
    lives_widget_show_all(omclw->dialog);

  return omclw;
}





static void init_omc_macros(void) {
  int i;

  for (i=0; i<N_OMC_MACROS; i++) {
    omc_macros[i].macro_text=NULL;
    omc_macros[i].info_text=NULL;
    omc_macros[i].msg=NULL;
    omc_macros[i].nparams=0;
    omc_macros[i].pname=NULL;
  }

  omc_macros[0].msg=lives_strdup("/video/play");
  omc_macros[0].macro_text=lives_strdup(_("Start video playback"));

  omc_macros[1].msg=lives_strdup("/video/stop");
  omc_macros[1].macro_text=lives_strdup(_("Stop video playback"));


  omc_macros[2].msg=lives_strdup("/clip/foreground/select");
  omc_macros[2].macro_text=lives_strdup(_("Clip select <clipnum>"));
  omc_macros[2].info_text=lives_strdup(_("Switch foreground clip to the nth valid clip"));
  omc_macros[2].nparams=1;

  omc_macros[3].msg=lives_strdup("/video/play/forwards");
  omc_macros[3].macro_text=lives_strdup(_("Play forwards"));
  omc_macros[3].info_text=lives_strdup(_("Play video in a forwards direction"));

  omc_macros[4].msg=lives_strdup("/video/play/backwards");
  omc_macros[4].macro_text=lives_strdup(_("Play backwards"));
  omc_macros[4].info_text=lives_strdup(_("Play video in a backwards direction"));

  omc_macros[5].msg=lives_strdup("/video/play/reverse");
  omc_macros[5].macro_text=lives_strdup(_("Reverse playback direction"));
  omc_macros[5].info_text=lives_strdup(_("Reverse direction of video playback"));

  omc_macros[6].msg=lives_strdup("/video/play/faster");
  omc_macros[6].macro_text=lives_strdup(_("Play video faster"));
  omc_macros[6].info_text=lives_strdup(_("Play video at a slightly faster rate"));

  omc_macros[7].msg=lives_strdup("/video/play/slower");
  omc_macros[7].macro_text=lives_strdup(_("Play video slower"));
  omc_macros[7].info_text=lives_strdup(_("Play video at a slightly slower rate"));

  omc_macros[8].msg=lives_strdup("/video/freeze/toggle");
  omc_macros[8].macro_text=lives_strdup(_("Toggle video freeze"));
  omc_macros[8].info_text=lives_strdup(_("Freeze video, or if already frozen, unfreeze it"));

  omc_macros[9].msg=lives_strdup("/video/fps/set");
  omc_macros[9].macro_text=lives_strdup(_("Set video framerate to <fps>"));
  omc_macros[9].info_text=lives_strdup(_("Set framerate of foreground clip to <float fps>"));
  omc_macros[9].nparams=1;

  omc_macros[10].msg=lives_strdup("/record/enable");
  omc_macros[10].macro_text=lives_strdup(_("Start recording"));

  omc_macros[11].msg=lives_strdup("/record/disable");
  omc_macros[11].macro_text=lives_strdup(_("Stop recording"));

  omc_macros[12].msg=lives_strdup("/record/toggle");
  omc_macros[12].macro_text=lives_strdup(_("Toggle recording state"));

  omc_macros[13].msg=lives_strdup("/clip/foreground/background/swap");
  omc_macros[13].macro_text=lives_strdup(_("Swap foreground and background clips"));
  omc_macros[14].msg=lives_strdup("/effect_key/reset");
  omc_macros[14].macro_text=lives_strdup(_("Reset effect keys"));
  omc_macros[14].info_text=lives_strdup(_("Switch all effects off."));

  omc_macros[15].msg=lives_strdup("/effect_key/enable");
  omc_macros[15].macro_text=lives_strdup(_("Enable effect key <key>"));
  omc_macros[15].nparams=1;

  omc_macros[16].msg=lives_strdup("/effect_key/disable");
  omc_macros[16].macro_text=lives_strdup(_("Disable effect key <key>"));
  omc_macros[16].nparams=1;

  omc_macros[17].msg=lives_strdup("/effect_key/toggle");
  omc_macros[17].macro_text=lives_strdup(_("Toggle effect key <key>"));
  omc_macros[17].nparams=1;

  omc_macros[18].msg=lives_strdup("/effect_key/nparameter/value/set");
  omc_macros[18].macro_text=lives_strdup(_("Set parameter value <key> <pnum> = <value>"));
  omc_macros[18].info_text=lives_strdup(_("Set <value> of pth (numerical) parameter for effect key <key>."));
  omc_macros[18].nparams=3;

  omc_macros[19].msg=lives_strdup("/clip/select/next");
  omc_macros[19].macro_text=lives_strdup(_("Switch foreground to next clip"));

  omc_macros[20].msg=lives_strdup("/clip/select/previous");
  omc_macros[20].macro_text=lives_strdup(_("Switch foreground to previous clip"));

  omc_macros[21].msg=lives_strdup("/video/fps/ratio/set");
  omc_macros[21].macro_text=lives_strdup(_("Set video framerate to ratio <fps_ratio>"));
  omc_macros[21].info_text=lives_strdup(_("Set framerate ratio of foreground clip to <float fps_ratio>"));
  omc_macros[21].nparams=1;

  omc_macros[22].msg=lives_strdup("/clip/foreground/retrigger");
  omc_macros[22].macro_text=lives_strdup(_("Retrigger clip <clipnum>"));
  omc_macros[22].info_text=lives_strdup(_("Switch foreground clip to the nth valid clip, and reset the frame number"));
  omc_macros[22].nparams=1;

  omc_macros[23].msg=lives_strdup("/effect_key/mode/next");
  omc_macros[23].macro_text=lives_strdup(_("Cycle to next mode for effect key <key>"));
  omc_macros[23].nparams=1;

  omc_macros[24].msg=lives_strdup("/effect_key/mode/previous");
  omc_macros[24].macro_text=lives_strdup(_("Cycle to previous mode for effect key <key>"));
  omc_macros[24].nparams=1;

  omc_macros[25].msg=lives_strdup("/video/play/parameter/value/set");
  omc_macros[25].macro_text=lives_strdup(_("Set playback plugin parameter value <pnum> = <value>"));
  omc_macros[25].info_text=lives_strdup(_("Set <value> of pth parameter for the playback plugin."));
  omc_macros[25].nparams=2;


  for (i=0; i<N_OMC_MACROS; i++) {
    if (omc_macros[i].msg!=NULL) {
      if (omc_macros[i].nparams>0) {
        omc_macros[i].ptypes=(int *)lives_malloc(omc_macros[i].nparams*sizint);
        omc_macros[i].mini=(int *)lives_malloc(omc_macros[i].nparams*sizint);
        omc_macros[i].maxi=(int *)lives_malloc(omc_macros[i].nparams*sizint);
        omc_macros[i].vali=(int *)lives_malloc(omc_macros[i].nparams*sizint);

        omc_macros[i].mind=(double *)lives_malloc(omc_macros[i].nparams*sizdbl);
        omc_macros[i].maxd=(double *)lives_malloc(omc_macros[i].nparams*sizdbl);
        omc_macros[i].vald=(double *)lives_malloc(omc_macros[i].nparams*sizdbl);
        omc_macros[i].pname=(char **)lives_malloc(omc_macros[i].nparams*sizeof(char *));

      }
    }
  }


  // clip select
  omc_macros[2].ptypes[0]=OMC_PARAM_INT;
  omc_macros[2].mini[0]=omc_macros[2].vali[0]=1;
  omc_macros[2].maxi[0]=100000;
  // TRANSLATORS: short form of "clip number"
  omc_macros[2].pname[0]=lives_strdup(_("clipnum"));


  // set fps (will be handled to avoid 0.)
  omc_macros[9].ptypes[0]=OMC_PARAM_DOUBLE;
  omc_macros[9].mind[0]=-200.;
  omc_macros[9].vald[0]=25.;
  omc_macros[9].maxd[0]=200.;
  // TRANSLATORS: short form of "frames per second"
  omc_macros[9].pname[0]=lives_strdup(_("fps"));

  // effect_key enable,disable, toggle
  omc_macros[15].ptypes[0]=OMC_PARAM_INT;
  omc_macros[15].mini[0]=1;
  omc_macros[15].vali[0]=1;
  omc_macros[15].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[15].pname[0]=lives_strdup(_("key"));

  omc_macros[16].ptypes[0]=OMC_PARAM_INT;
  omc_macros[16].mini[0]=1;
  omc_macros[16].vali[0]=1;
  omc_macros[16].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[16].pname[0]=lives_strdup(_("key"));

  omc_macros[17].ptypes[0]=OMC_PARAM_INT;
  omc_macros[17].mini[0]=1;
  omc_macros[17].vali[0]=1;
  omc_macros[17].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[17].pname[0]=lives_strdup(_("key"));

  // key
  omc_macros[18].ptypes[0]=OMC_PARAM_INT;
  omc_macros[18].mini[0]=1;
  omc_macros[18].vali[0]=1;
  omc_macros[18].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[18].pname[0]=lives_strdup(_("key"));

  // param (this will be matched with numeric params)
  omc_macros[18].ptypes[1]=OMC_PARAM_INT;
  omc_macros[18].mini[1]=0;
  omc_macros[18].maxi[1]=32;
  omc_macros[18].vali[1]=0;
  // TRANSLATORS: short form of "parameter number"
  omc_macros[18].pname[1]=lives_strdup(_("pnum"));

  // value (this will get special handling)
  // type conversion and auto offset/scaling will be done
  omc_macros[18].ptypes[2]=OMC_PARAM_SPECIAL;
  omc_macros[18].mind[2]=0.;
  omc_macros[18].maxd[2]=0.;
  omc_macros[18].vald[2]=0.;
  omc_macros[18].pname[2]=lives_strdup(_("value"));

  // set ratio fps (will be handled to avoid 0.)
  omc_macros[21].ptypes[0]=OMC_PARAM_DOUBLE;
  omc_macros[21].mind[0]=-10.;
  omc_macros[21].vald[0]=1.;
  omc_macros[21].maxd[0]=10.;
  // TRANSLATORS: short form of "frames per second"
  omc_macros[21].pname[0]=lives_strdup(_("fps_ratio"));


  // clip retrigger
  omc_macros[22].ptypes[0]=OMC_PARAM_INT;
  omc_macros[22].mini[0]=omc_macros[22].vali[0]=1;
  omc_macros[22].maxi[0]=100000;
  // TRANSLATORS: short form of "clip number"
  omc_macros[22].pname[0]=lives_strdup(_("clipnum"));

  // key
  omc_macros[23].ptypes[0]=OMC_PARAM_INT;
  omc_macros[23].mini[0]=1;
  omc_macros[23].vali[0]=1;
  omc_macros[23].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[23].pname[0]=lives_strdup(_("key"));

  // key
  omc_macros[24].ptypes[0]=OMC_PARAM_INT;
  omc_macros[24].mini[0]=1;
  omc_macros[24].vali[0]=1;
  omc_macros[24].maxi[0]=prefs->rte_keys_virtual;
  // TRANSLATORS: as in keyboard key
  omc_macros[24].pname[0]=lives_strdup(_("key"));


  // param
  omc_macros[25].ptypes[0]=OMC_PARAM_INT;
  omc_macros[25].mini[0]=0;
  omc_macros[25].maxi[0]=128;
  omc_macros[25].vali[0]=0;
  // TRANSLATORS: short form of "parameter number"
  omc_macros[25].pname[0]=lives_strdup(_("pnum"));

  // value (this will get special handling)
  // type conversion and auto offset/scaling will be done
  omc_macros[25].ptypes[1]=OMC_PARAM_SPECIAL;
  omc_macros[25].mind[1]=0.;
  omc_macros[25].maxd[1]=0.;
  omc_macros[25].vald[1]=0.;
  omc_macros[25].pname[1]=lives_strdup(_("value"));

}


static int get_nfixed(int type, const char *string) {
  int nfixed=0;

  switch (type) {
  case OMC_JS_BUTTON:
    nfixed=3; // type, index, value
    break;
  case OMC_JS_AXIS:
    nfixed=2;  // type, index
    break;
#ifdef OMC_MIDI_IMPL
  case OMC_MIDI:
    type=midi_msg_type(string);
    return get_nfixed(type,NULL);
  case OMC_MIDI_CONTROLLER:
    nfixed=3;     // type, channel, cnum
    break;
  case OMC_MIDI_NOTE:
  case OMC_MIDI_NOTE_OFF:
  case OMC_MIDI_PITCH_BEND:
  case OMC_MIDI_PGM_CHANGE:
    nfixed=2; // type, channel
    break;
#endif
  }
  return nfixed;
}



static boolean match_filtered_params(lives_omc_match_node_t *mnode, const char *sig, int nfixed) {
  int i;
  char **array=lives_strsplit(sig," ",-1);

  for (i=0; i<mnode->nvars; i++) {
    if (mnode->matchp[i]) {
      if (mnode->matchi[i]!=atoi(array[nfixed+i])) {
        //g_print("data mismatch %d %d %d\n",mnode->matchi[i],atoi(array[nfixed+i]),nfixed);
        lives_strfreev(array);
        return FALSE;
      }
    }
  }
  //g_print("data match\n");
  lives_strfreev(array);
  return TRUE;
}




static lives_omc_match_node_t *omc_match_sig(int type, int index, const char *sig) {
  LiVESSList *nlist=omc_node_list;
  char *srch,*cnodex;
  lives_omc_match_node_t *cnode;
  int nfixed;

  if (type==OMC_MIDI) {
    if (index==-1) srch=lives_strdup_printf("%d %s ",type,sig);
    else srch=lives_strdup_printf("%d %d %s ",type,index,sig);
  } else srch=lives_strdup_printf("%s ",sig);

  nfixed=get_nfixed(type,sig);

  while (nlist!=NULL) {
    cnode=(lives_omc_match_node_t *)nlist->data;
    cnodex=lives_strdup_printf("%s ",cnode->srch);
    //g_print("cf %s and %s\n",cnode->srch,srch);
    if (!strncmp(cnodex,srch,strlen(cnodex))) {
      // got a possible match
      // now check the data
      if (match_filtered_params(cnode,sig,nfixed)) {
        lives_free(srch);
        lives_free(cnodex);
        return cnode;
      }
    }
    nlist=nlist->next;
    lives_free(cnodex);
  }
  lives_free(srch);
  return NULL;
}


/* not used yet */
/*static char *omclearn_request_min(int type) {
  char *msg=NULL;

  switch (type) {
  case OMC_JS_AXIS:
    msg=lives_strdup(_("\n\nNow move the stick to the opposite position and click OK\n\n"));
    break;
  case OMC_MIDI_CONTROLLER:
    msg=lives_strdup(_("\n\nPlease set the control to its minimum value and click OK\n\n"));
    break;
  case OMC_MIDI_NOTE:
    msg=lives_strdup(_("\n\nPlease release the note\n\n"));
    break;
  }

  do_blocking_error_dialog(msg);
  if (msg!=NULL) lives_free(msg);


  return NULL;
  }*/



static LIVES_INLINE int omclearn_get_fixed_elems(const char *string1, const char *string2) {
  // count how many (non-space) elements match
  // e.g "a b c" and "a b d" returns 2

  // neither string may end in a space

  register int i;

  int match=0;
  int stlen=MIN(strlen(string1),strlen(string2));

  for (i=0; i<stlen; i++) {
    if (strcmp((string1+i),(string2+i))) return match;
    if (!strcmp((string1+i)," ")) match++;
  }

  return match+1;

}



static LIVES_INLINE int get_nth_elem(const char *string, int idx) {
  char **array=lives_strsplit(string," ",-1);
  int retval=atoi(array[idx]);
  lives_strfreev(array);
  return retval;
}





static lives_omc_match_node_t *lives_omc_match_node_new(int str_type, int index, const char *string, int nfixed) {
  int i;
  char *tmp;
  char *srch_str;
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)lives_malloc(sizeof(lives_omc_match_node_t));

  if (str_type==OMC_MIDI) {
    if (index>-1) srch_str=lives_strdup_printf("%d %d %s",str_type,index,(tmp=cut_string_elems(string,nfixed<0?-1:nfixed)));
    else srch_str=lives_strdup_printf("%d %s",str_type,(tmp=cut_string_elems(string,nfixed<0?-1:nfixed)));
    lives_free(tmp);
  } else {
    srch_str=lives_strdup_printf("%s",(tmp=cut_string_elems(string,nfixed<0?-1:nfixed)));
    lives_free(tmp);
  }

  //g_print("srch_str was %d %d .%s. %d\n",str_type,index,srch_str,nfixed);

  mnode->srch=srch_str;
  mnode->macro=-1;

  if (nfixed<0) mnode->nvars=-(nfixed+1);
  else mnode->nvars=get_token_count(string,' ')-nfixed;

  if (mnode->nvars>0) {
    mnode->offs0=(int *)lives_malloc(mnode->nvars*sizint);
    mnode->scale=(double *)lives_malloc(mnode->nvars*sizdbl);
    mnode->offs1=(int *)lives_malloc(mnode->nvars*sizint);
    mnode->min=(int *)lives_malloc(mnode->nvars*sizint);
    mnode->max=(int *)lives_malloc(mnode->nvars*sizint);
    mnode->matchp=(boolean *)lives_malloc(mnode->nvars*sizeof(boolean));
    mnode->matchi=(int *)lives_malloc(mnode->nvars*sizint);
  }

  for (i=0; i<mnode->nvars; i++) {
    mnode->offs0[i]=mnode->offs1[i]=0;
    mnode->scale[i]=1.;
    mnode->matchp[i]=FALSE;
  }

  mnode->map=mnode->fvali=NULL;
  mnode->fvald=NULL;

  mnode->treev1=mnode->treev2=NULL;
  mnode->gtkstore=mnode->gtkstore2=NULL;

  return mnode;
}




static int *omclearn_get_values(const char *string, int nfixed) {
  register int i,j;
  size_t slen,tslen;
  int *retvals,count=0,nvars;

  slen=strlen(string);

  nvars=get_token_count(string,' ')-nfixed;

  retvals=(int *)lives_malloc(nvars*sizint);

  for (i=0; i<slen; i++) {
    if (!strncmp((string+i)," ",1)) {
      if (--nfixed<=0) {
        char *tmp=lives_strdup(string+i+1);
        tslen=strlen(tmp);
        for (j=0; j<tslen; j++) {
          if (!strncmp((tmp+j)," ",1)) {
            memset(tmp+j,0,1);
            retvals[count++]=atoi(tmp);
            lives_free(tmp);
            break;
          }
        }
        if (j==tslen) {
          retvals[count++]=atoi(tmp);
          lives_free(tmp);
          return retvals;
        }
        i+=j;
      }
    }
  }

  // should never reach here
  return NULL;
}




void omclearn_match_control(lives_omc_match_node_t *mnode, int str_type, int index, const char *string, int nfixed, omclearn_w *omclw) {

  if (nfixed==-1) {
    // already there : allow user to update
    return;
  }

  if (index==-1) {
    index=get_nth_elem(string,1);
  }

  // add descriptive text on left
  // add combo box on right

  omc_learner_add_row(str_type,index,mnode,string,omclw);


}





lives_omc_match_node_t *omc_learn(const char *string, int str_type, int idx, omclearn_w *omclw) {
  // here we come with a string, which must be a sequence of integers
  // separated by single spaces

  // the str_type is one of JS_AXIS, JS_BUTTON, MIDI_CONTROLLER, MIDI_KEY, etc.

  // idx is -1, except for JS_BUTTON and JS_AXIS where it can be used

  // the string is first transformed into
  // signifier and value

  // next, we check if signifier is already matched to a macro

  // if not we allow the user to match it to any macro that has n or less parameters, where n is the number of variables in string


  lives_omc_match_node_t *mnode;

  int nfixed=get_nfixed(str_type,string);


  switch (str_type) {
  case OMC_MIDI_CONTROLLER:
    // display controller and allow it to be matched
    // then request min

    mnode=omc_match_sig(OMC_MIDI,idx,string);
    //g_print("autoscale !\n");

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);
      mnode->max[0]=127;
      mnode->min[0]=0;
      idx=midi_index(string);
      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);
      return mnode;
    }
    break;
  case OMC_MIDI_PGM_CHANGE:
    // display controller and allow it to be matched

    mnode=omc_match_sig(OMC_MIDI,idx,string);
    //g_print("autoscale !\n");

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);
      mnode->max[0]=127;
      mnode->min[0]=0;
      idx=midi_index(string);
      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);
      return mnode;
    }
    break;
  case OMC_MIDI_PITCH_BEND:
    // display controller and allow it to be matched
    // then request min

    mnode=omc_match_sig(OMC_MIDI,idx,string);
    //g_print("autoscale !\n");

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);
      mnode->max[0]=8192;
      mnode->min[0]=-8192;
      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);
      return mnode;
    }
    break;
  case OMC_MIDI_NOTE:
  case OMC_MIDI_NOTE_OFF:
    // display note and allow it to be matched
    mnode=omc_match_sig(OMC_MIDI,idx,string);

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);

      mnode->max[0]=127;
      mnode->min[0]=0;

      mnode->max[1]=127;
      mnode->min[1]=0;

      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);

      return mnode;
    }
    break;
  case OMC_JS_AXIS:
    // display axis and allow it to be matched
    // then request min

    mnode=omc_match_sig(str_type,idx,string);

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(str_type,idx,string,nfixed);

      mnode->min[0]=-128;
      mnode->max[0]=128;

      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);
      return mnode;
    }
    break;
  case OMC_JS_BUTTON:
    // display note and allow it to be matched
    mnode=omc_match_sig(str_type,idx,string);

    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(str_type,idx,string,nfixed);
      omclearn_match_control(mnode,str_type,idx,string,nfixed,omclw);
      return mnode;
    }
    break;
  default:
    // hmmm....

    break;
  }
  return NULL;
}



// here we process a string which is formed of (supertype) (type) [(idx)] [(values)]
// eg "val_for_js js_button idx_1  1"  => "2 3 1

// in learn mode we store the sting + its meaning

// in playback mode, we match the string with our database, and then convert/append the variables

boolean omc_process_string(int supertype, const char *string, boolean learn, omclearn_w *omclw) {
  // only need to set omclw if learn is TRUE

  // returns TRUE if we learn new, or if we carry out an action
  // retruns FALSE otherwise

  boolean ret=FALSE;
  int type=0,idx=-1;
  lives_omc_match_node_t *mnode;

  if (string==NULL)  return FALSE;

  switch (supertype) {
  case OMC_JS:
#ifdef OMC_JS_IMPL
    supertype=type=js_msg_type(string);
    idx=js_index(string);
#endif
    break;
#ifdef OMC_MIDI_IMPL
  case OMC_MIDI:
    type=midi_msg_type(string);
    //idx=midi_index(string);
    idx=-1;
#endif
  }
  if (type>0) {
    if (learn) {
      // pass to learner
      mnode=omc_learn(string,type,idx,omclw);
      if (mnode!=NULL) {
        ret=TRUE;
        omc_node_list=lives_slist_append(omc_node_list,mnode);
      }
    } else {
      OSCbuf *oscbuf=omc_learner_decode(supertype,idx,string);

      // if not playing, the only commands we allow are:
      // /video/play
      // /clip/foreground/retrigger
      // and enabling a generator

      // basically only messages which will trigger start of playback


      // further checks are performed when enabling/toggling an effect to see whether it is a generator

      if (oscbuf!=NULL) {
        if (mainw->playing_file==-1
            &&strcmp(oscbuf->buffer,"/video/play")
            &&strcmp(oscbuf->buffer,"/clip/foreground/retrigger")
            &&strcmp(oscbuf->buffer,"/effect_key/enable")
            &&strcmp(oscbuf->buffer,"/effect_key/toggle")
           ) return FALSE;

        lives_osc_act(oscbuf);
        ret=TRUE;
      }
    }
  }
  return ret;
}





void on_midi_learn_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  omclearn_w *omclw=create_omclearn_dialog();
  char *string=NULL;

  if (!omc_macros_inited) {
    init_omc_macros();
    omc_macros_inited=TRUE;
    OSC_initBuffer(&obuf,OSC_BUF_SIZE,byarr);
  }

#ifdef OMC_MIDI_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_MIDI]) midi_open();
#endif

#ifdef OMC_JS_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_JS]) js_open();
#endif

  lives_widget_show(omclw->dialog);

  mainw->cancelled=CANCEL_NONE;

  show_existing(omclw);

  // read controls and notes
  while (mainw->cancelled==CANCEL_NONE) {
    // read from devices

#ifdef OMC_JS_IMPL
    if (mainw->ext_cntl[EXT_CNTL_JS]) string=js_mangle();
    if (string!=NULL) {
      omc_process_string(OMC_JS,string,TRUE,omclw);
      lives_free(string);
      string=NULL;
    } else {
#endif

#ifdef OMC_MIDI_IMPL
      if (mainw->ext_cntl[EXT_CNTL_MIDI]) string=midi_mangle();
      //#define TEST_OMC_LEARN
#ifdef TEST_OMC_LEARN
      string=lives_strdup("176 10 0 1");
#endif
      if (string!=NULL) {
        omc_process_string(OMC_MIDI,string,TRUE,omclw);
        lives_free(string);
        string=NULL;
      }
#endif

#ifdef OMC_JS_IMPL
    }
#endif

    lives_usleep(prefs->sleep_time);

    lives_widget_context_update();
  }

  remove_all_nodes(FALSE,omclw);

  lives_widget_destroy(omclw->dialog);

  mainw->cancelled=CANCEL_NONE;

  lives_free(omclw);

}





static void write_fx_tag(const char *string, int nfixed, lives_omc_match_node_t *mnode, lives_omc_macro_t *omacro, char *typetags) {
  // get typetag for a filter parameter

  int i,j,k;
  int *vals=omclearn_get_values(string,nfixed);
  int oval0=1,oval1=0;

  for (i=0; i<omacro->nparams; i++) {
    // get fixed val or map from
    j=mnode->map[i];

    if (j>-1) {
      if (i==2) {
        // auto scale for fx param
        int error,ntmpls,hint,flags;
        int mode=rte_key_getmode(oval0);
        weed_plant_t *filter;
        weed_plant_t **ptmpls;
        weed_plant_t *ptmpl;

        if (mode==-1) return;

        filter=rte_keymode_get_filter(oval0,mode);

        ntmpls=weed_leaf_num_elements(filter,"in_parameter_templates");

        ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
        for (k=0; k<ntmpls; k++) {
          ptmpl=ptmpls[k];
          if (weed_plant_has_leaf(ptmpl,"host_internal_connection")) continue;
          hint=weed_get_int_value(ptmpl,"hint",&error);
          flags=weed_get_int_value(ptmpl,"flags",&error);
          if (flags&WEED_PARAMETER_VARIABLE_ELEMENTS) flags^=WEED_PARAMETER_VARIABLE_ELEMENTS;
          if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(ptmpl,"default")==1) {
            if (oval1==0) {
              if (hint==WEED_HINT_INTEGER) {
                // **int
                lives_strappend(typetags,OSC_MAX_TYPETAGS,"i");
              } else {
                // float
                lives_strappend(typetags,OSC_MAX_TYPETAGS,"f");
              }
            }
            oval1--;
          }
        }
        lives_free(ptmpls);
      } else {
        // playback plugin params
        if (omacro->ptypes[i]==OMC_PARAM_INT) {
          int oval=myround((double)(vals[j]+mnode->offs0[j])*mnode->scale[j])+mnode->offs1[j];
          if (i==0) oval0=oval;
          if (i==1) oval1=oval;
        }
      }
    } else {
      if (omacro->ptypes[i]==OMC_PARAM_INT) {
        if (i==0) oval0=mnode->fvali[i];
        if (i==1) oval1=mnode->fvali[i];
      }
    }
  }
  lives_free(vals);
}










OSCbuf *omc_learner_decode(int type, int idx, const char *string) {
  int macro,nfixed;
  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;
  int oval0=1,oval1=0;
  int error,ntmpls,hint,flags;

  register int i,j,k;

  int *vals;

  char typetags[OSC_MAX_TYPETAGS];

  mnode=omc_match_sig(type,idx,string);

  if (mnode==NULL) return NULL;

  macro=mnode->macro;

  if (macro==-1) return NULL;

  omacro=omc_macros[macro];

  if (omacro.msg==NULL) return NULL;

  OSC_resetBuffer(&obuf);

  lives_snprintf(typetags,OSC_MAX_TYPETAGS,",");

  nfixed=get_token_count(string,' ')-mnode->nvars;

  // get typetags
  for (i=0; i<omacro.nparams; i++) {
    if (omacro.ptypes[i]==OMC_PARAM_SPECIAL) {
      write_fx_tag(string,nfixed,mnode,&omacro,typetags);
    } else {
      if (omacro.ptypes[i]==OMC_PARAM_INT) lives_strappend(typetags,OSC_MAX_TYPETAGS,"i");
      else lives_strappend(typetags,OSC_MAX_TYPETAGS,"f");
    }
  }

  OSC_writeAddressAndTypes(&obuf,omacro.msg,typetags);


  if (omacro.nparams>0) {

    vals=omclearn_get_values(string,nfixed);

    for (i=0; i<omacro.nparams; i++) {
      // get fixed val or map from
      j=mnode->map[i];

      if (j>-1) {

        if (macro==25 && i==1 && mainw->vpp!=NULL && mainw->vpp->play_params!=NULL && oval0<mainw->vpp->num_play_params) {
          // auto scale for playback plugin params

          weed_plant_t *ptmpl=weed_get_plantptr_value((weed_plant_t *)pp_get_param(mainw->vpp->play_params,oval0),"template",&error);
          hint=weed_get_int_value(ptmpl,"hint",&error);
          if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&weed_leaf_num_elements(ptmpl,"default")==1) {
            if (hint==WEED_HINT_INTEGER) {
              int omin=mnode->min[j];
              int omax=mnode->max[j];
              int mini=weed_get_int_value(ptmpl,"min",&error);
              int maxi=weed_get_int_value(ptmpl,"max",&error);

              int oval=(int)((double)(vals[j]-omin)/(double)(omax-omin)*(double)(maxi-mini))+mini;
              OSC_writeIntArg(&obuf,oval);
            } else {
              // float
              int omin=mnode->min[j];
              int omax=mnode->max[j];
              double minf=weed_get_double_value(ptmpl,"min",&error);
              double maxf=weed_get_double_value(ptmpl,"max",&error);

              double oval=(double)(vals[j]-omin)/(double)(omax-omin)*(maxf-minf)+minf;
              OSC_writeFloatArg(&obuf,(float)oval);
            } // end float
          }
        } else {
          if (macro==18&&i==2) {
            // auto scale for fx param
            int mode=rte_key_getmode(oval0);
            weed_plant_t *filter;
            weed_plant_t **ptmpls;
            weed_plant_t *ptmpl;

            if (mode==-1) return NULL;

            filter=rte_keymode_get_filter(oval0,mode);

            ntmpls=weed_leaf_num_elements(filter,"in_parameter_templates");

            ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
            for (k=0; k<ntmpls; k++) {
              ptmpl=ptmpls[k];
              if (weed_plant_has_leaf(ptmpl,"host_internal_connection")) continue;
              hint=weed_get_int_value(ptmpl,"hint",&error);
              flags=weed_get_int_value(ptmpl,"flags",&error);
              if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(ptmpl,"default")==1) {
                if (oval1==0) {
                  if (hint==WEED_HINT_INTEGER) {
                    int omin=mnode->min[j];
                    int omax=mnode->max[j];
                    int mini=weed_get_int_value(ptmpl,"min",&error);
                    int maxi=weed_get_int_value(ptmpl,"max",&error);

                    int oval=(int)((double)(vals[j]-omin)/(double)(omax-omin)*(double)(maxi-mini))+mini;
                    OSC_writeIntArg(&obuf,oval);
                  } else {
                    // float
                    int omin=mnode->min[j];
                    int omax=mnode->max[j];
                    double minf=weed_get_double_value(ptmpl,"min",&error);
                    double maxf=weed_get_double_value(ptmpl,"max",&error);

                    double oval=(double)(vals[j]-omin)/(double)(omax-omin)*(maxf-minf)+minf;
                    OSC_writeFloatArg(&obuf,(float)oval);
                  } // end float
                }
                oval1--;
              }
            }
            lives_free(ptmpls);
          } else {
            if (omacro.ptypes[i]==OMC_PARAM_INT) {
              int oval=myround((double)(vals[j]+mnode->offs0[j])*mnode->scale[j])+mnode->offs1[j];
              if (i==0) oval0=oval;
              if (i==1) oval1=oval;
              OSC_writeIntArg(&obuf,oval);
            } else {
              double oval=(double)(vals[j]+mnode->offs0[j])*mnode->scale[j]+(double)mnode->offs1[j];
              OSC_writeFloatArg(&obuf,oval);
            }
          }
        }
      } else {
        if (omacro.ptypes[i]==OMC_PARAM_INT) {
          OSC_writeIntArg(&obuf,mnode->fvali[i]);
          if (i==0) oval0=mnode->fvali[i];
          if (i==1) oval1=mnode->fvali[i];
        } else {
          OSC_writeFloatArg(&obuf,(float)mnode->fvald[i]);
        }
      }
    }
    lives_free(vals);
  }

  return &obuf;
}






/////////////////////////////////////

/** Save midi mapping to an external file
 */


void on_midi_save_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  LiVESSList *slist=omc_node_list;

  size_t srchlen;

  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;

  char *save_file;

  int nnodes;
  int retval;

  int fd;

  register int i;

  save_file=choose_file(NULL,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);

  if (save_file==NULL||!strlen(save_file)) return;

  d_print(_("Saving device mapping to file %s..."),save_file);

  do {
    retval=0;
    if ((fd=open(save_file,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))<0) {
      retval=do_write_failed_error_s_with_retry(save_file,lives_strerror(errno),NULL);
      if (retval==LIVES_RESPONSE_CANCEL) {
        lives_free(save_file);
        d_print_failed();
        return;
      }
    } else {
      mainw->write_failed=FALSE;

      lives_write(fd,OMC_FILE_VSTRING,strlen(OMC_FILE_VSTRING),TRUE);

      nnodes=lives_slist_length(omc_node_list);
      lives_write_le(fd,&nnodes,4,TRUE);

      while (slist!=NULL) {
        if (mainw->write_failed) break;
        mnode=(lives_omc_match_node_t *)slist->data;
        srchlen=strlen(mnode->srch);

        lives_write_le(fd,&srchlen,4,TRUE);
        lives_write(fd,mnode->srch,srchlen,TRUE);

        lives_write_le(fd,&mnode->macro,4,TRUE);
        lives_write_le(fd,&mnode->nvars,4,TRUE);

        for (i=0; i<mnode->nvars; i++) {
          if (mainw->write_failed) break;
          lives_write_le(fd,&mnode->offs0[i],4,TRUE);
          lives_write_le(fd,&mnode->scale[i],8,TRUE);
          lives_write_le(fd,&mnode->offs1[i],4,TRUE);

          lives_write_le(fd,&mnode->min[i],4,TRUE);
          lives_write_le(fd,&mnode->max[i],4,TRUE);

          lives_write_le(fd,&mnode->matchp[i],4,TRUE);
          lives_write_le(fd,&mnode->matchi[i],4,TRUE);
        }

        omacro=omc_macros[mnode->macro];

        for (i=0; i<omacro.nparams; i++) {
          if (mainw->write_failed) break;
          lives_write_le(fd,&mnode->map[i],4,TRUE);
          lives_write_le(fd,&mnode->fvali[i],4,TRUE);
          lives_write_le(fd,&mnode->fvald[i],8,TRUE);
        }
        slist=slist->next;
      }

      close(fd);

      if (mainw->write_failed) {
        retval=do_write_failed_error_s_with_retry(save_file,NULL,NULL);
        if (retval==LIVES_RESPONSE_CANCEL) d_print_file_error_failed();
      }
    }
  } while (retval==LIVES_RESPONSE_RETRY);

  if (retval!=LIVES_RESPONSE_CANCEL) d_print_done();

  lives_free(save_file);

}


static void omc_node_list_free(LiVESSList *slist) {
  while (slist!=NULL) {
    omc_match_node_free((lives_omc_match_node_t *)slist->data);
    slist=slist->next;
  }
  lives_slist_free(slist);
  slist=NULL;
}


static void do_midi_load_error(const char *fname) {
  char *msg=lives_strdup_printf(_("\n\nError parsing file\n%s\n"),fname);
  do_blocking_error_dialog(msg);
  lives_free(msg);
  d_print_failed();
}

static void do_midi_version_error(const char *fname) {
  char *msg=lives_strdup_printf(_("\n\nInvalid version in file\n%s\n"),fname);
  do_blocking_error_dialog(msg);
  lives_free(msg);
  d_print_failed();
}




void on_midi_load_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;

  ssize_t bytes;

  char tstring[512];

  char *load_file=NULL;
  char *srch;

  uint32_t srchlen,nnodes,macro,nvars,supertype;
  int idx=-1;
  int fd;

  register int i,j;

#ifdef OMC_MIDI_IMPL
  size_t blen;
  char *tmp;
#endif

  if (user_data==NULL) load_file=choose_file(NULL,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_OPEN,NULL,NULL);
  else load_file=lives_strdup((char *)user_data);

  if (load_file==NULL||!strlen(load_file)) return;

  d_print(_("Loading device mapping from file %s..."),load_file);

  if ((fd=open(load_file,O_RDONLY))<0) {
    char *msg=lives_strdup_printf(_("\n\nUnable to open file\n%s\nError code %d\n"),load_file,errno);
    do_blocking_error_dialog(msg);
    lives_free(msg);
    lives_free(load_file);
    d_print_failed();
    return;
  }

  if (!omc_macros_inited) {
    init_omc_macros();
    omc_macros_inited=TRUE;
    OSC_initBuffer(&obuf,OSC_BUF_SIZE,byarr);
  }

  bytes=read(fd,tstring,strlen(OMC_FILE_VSTRING));
  if (bytes<strlen(OMC_FILE_VSTRING)) {
    do_midi_load_error(load_file);
    lives_free(load_file);
    close(fd);
    return;
  }

  if (strncmp(tstring,OMC_FILE_VSTRING,strlen(OMC_FILE_VSTRING))) {
    do_midi_version_error(load_file);
    lives_free(load_file);
    close(fd);
    return;
  }

  bytes=lives_read_le(fd,&nnodes,4,TRUE);
  if (bytes<4) {
    do_midi_load_error(load_file);
    lives_free(load_file);
    close(fd);
    return;
  }

  if (omc_node_list!=NULL) {
    omc_node_list_free(omc_node_list);
    omc_node_list=NULL;
  }

  for (i=0; i<nnodes; i++) {

    bytes=lives_read_le(fd,&srchlen,4,TRUE);
    if (bytes<4) {
      do_midi_load_error(load_file);
      lives_free(load_file);
      close(fd);
      return;
    }

    srch=(char *)lives_malloc(srchlen+1);

    bytes=read(fd,srch,srchlen);
    if (bytes<srchlen) {
      do_midi_load_error(load_file);
      lives_free(load_file);
      close(fd);
      return;
    }

    memset(srch+srchlen,0,1);

    bytes=lives_read_le(fd,&macro,4,TRUE);
    if (bytes<sizint) {
      do_midi_load_error(load_file);
      lives_free(load_file);
      lives_free(srch);
      close(fd);
      return;
    }

    bytes=lives_read_le(fd,&nvars,4,TRUE);
    if (bytes<4) {
      do_midi_load_error(load_file);
      lives_free(load_file);
      lives_free(srch);
      close(fd);
      return;
    }

    supertype=atoi(srch);

    switch (supertype) {
#ifdef OMC_JS_IMPL
    case OMC_JS:
      supertype=js_msg_type(srch);
    case OMC_JS_BUTTON:
    case OMC_JS_AXIS:
      idx=js_index(srch);
      break;
#endif
#ifdef OMC_MIDI_IMPL
    case OMC_MIDI:
      idx=-1;

      // cut first value (supertype) as we will be added back in match_node_new
      tmp=cut_string_elems(srch,1);
      blen=strlen(tmp);
      tmp=lives_strdup(srch+blen+1);
      lives_free(srch);
      srch=tmp;

      break;
#endif
    default:
      return;
    }

    mnode=lives_omc_match_node_new(supertype,idx,srch,-(nvars+1));
    lives_free(srch);

    mnode->macro=macro;

    for (j=0; j<nvars; j++) {
      bytes=lives_read_le(fd,&mnode->offs0[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=lives_read_le(fd,&mnode->scale[j],8,TRUE);
      if (bytes<8) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=lives_read_le(fd,&mnode->offs1[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }

      bytes=lives_read_le(fd,&mnode->min[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=lives_read_le(fd,&mnode->max[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }

      bytes=lives_read_le(fd,&mnode->matchp[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=lives_read_le(fd,&mnode->matchi[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
    }

    omacro=omc_macros[macro];

    mnode->map=(int *)lives_malloc(omacro.nparams*sizint);
    mnode->fvali=(int *)lives_malloc(omacro.nparams*sizint);
    mnode->fvald=(double *)lives_malloc(omacro.nparams*sizdbl);

    for (j=0; j<omacro.nparams; j++) {
      bytes=lives_read_le(fd,&mnode->map[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=lives_read_le(fd,&mnode->fvali[j],4,TRUE);
      if (bytes<4) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
      bytes=read(fd,&mnode->fvald[j],8);
      if (bytes<8) {
        do_midi_load_error(load_file);
        lives_free(load_file);
        close(fd);
        return;
      }
    }
    omc_node_list=lives_slist_append(omc_node_list,(livespointer)mnode);
  }

  close(fd);
  d_print_done();


#ifdef OMC_MIDI_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_MIDI]) midi_open();
#endif

#ifdef OMC_JS_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_JS]) js_open();
#endif

}


#endif
