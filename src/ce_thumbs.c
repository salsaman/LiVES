// ce_thumbs.c
// LiVES
// (c) G. Finch 2013 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// clip thumbnails window for dual head mode

// TODO - buttons for some keys ?

// TODO - drag fx order :  check for data conx

// TODO - user defined screen mapping areas


#include "main.h"
#include "effects-weed.h"
#include "effects.h"
#include "paramwindow.h"
#include "ce_thumbs.h"

static LiVESWidget **fxcombos;
static LiVESWidget **pscrolls;
static LiVESWidget **combo_entries;
static LiVESWidget **key_checks;
static LiVESWidget **rb_fx_areas;
static LiVESWidget **rb_clip_areas;
static LiVESWidget **clip_boxes;
static LiVESWidget *param_hbox;
static LiVESWidget *top_hbox;
static ulong *ch_fns;
static ulong *combo_fns;
static ulong *rb_clip_fns;
static ulong *rb_fx_fns;

static int rte_keys_virtual;
static int n_screen_areas;
static int n_clip_boxes;

static int next_screen_area;

static void ce_thumbs_remove_param_boxes(boolean remove_pinned);
static void ce_thumbs_remove_param_box(int key);


void ce_thumbs_set_interactive(boolean interactive) {
  register int i;

  if (!interactive) {
    for (i=0; i<rte_keys_virtual; i++) {
      lives_widget_set_sensitive(fxcombos[i],FALSE);
      lives_widget_set_sensitive(key_checks[i],FALSE);
    }
  } else {
    for (i=0; i<rte_keys_virtual; i++) {
      lives_widget_set_sensitive(fxcombos[i],TRUE);
      if (rte_key_getmaxmode(i+1)>0) {
        lives_widget_set_sensitive(key_checks[i],TRUE);
      }
    }
  }

}

#if LIVES_HAS_GRID_WIDGET
static boolean switch_clip_cb(LiVESWidget *eventbox, LiVESXEventButton *event, livespointer user_data) {
  int i=LIVES_POINTER_TO_INT(user_data);
  if (mainw->playing_file==-1) return FALSE;
  if (!mainw->interactive) return FALSE;
  switch_clip(0,i,FALSE);
  return FALSE;
}

static void ce_thumbs_fx_changed(LiVESCombo *combo, livespointer user_data) {
  // callback after user switches fx via combo
  int key=LIVES_POINTER_TO_INT(user_data);
  int mode,cmode;

  if ((mode=lives_combo_get_active(combo))==-1) return; // -1 is returned after we set our own text (without the type)
  cmode=rte_key_getmode(key+1);

  if (cmode==mode) return;

  lives_widget_grab_focus(combo_entries[key]);

  rte_key_setmode(key+1,mode);
}
#endif


void ce_thumbs_set_key_check_state(void) {
  // set (delayed) keycheck state
  register int i;
  for (i=0; i<prefs->rte_keys_virtual; i++) {
    lives_signal_handler_block(key_checks[i],ch_fns[i]);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_checks[i]),
                                   LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(key_checks[i]),"active")));
    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(key_checks[i]))&&pscrolls[i]!=NULL) ce_thumbs_remove_param_box(i);
    lives_signal_handler_unblock(key_checks[i],ch_fns[i]);
  }
}


void ce_thumbs_set_keych(int key, boolean on) {
  // set key check from other source
  if (key>=rte_keys_virtual) return;
  lives_signal_handler_block(key_checks[key],ch_fns[key]);
  if (!pthread_mutex_trylock(&mainw->gtk_mutex)) {
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_checks[key]),on);
    if (!on&&pscrolls[key]!=NULL) ce_thumbs_remove_param_box(key);
    pthread_mutex_unlock(&mainw->gtk_mutex);
  }
  lives_signal_handler_unblock(key_checks[key],ch_fns[key]);
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(key_checks[key]),"active",LIVES_INT_TO_POINTER(on));
}


void ce_thumbs_set_mode_combo(int key, int mode) {
  // set combo from other source : need to add params after
  if (key>=rte_keys_virtual) return;
  if (mode<0) return;
  lives_signal_handler_block(fxcombos[key],combo_fns[key]);
  lives_combo_set_active_index(LIVES_COMBO(fxcombos[key]),mode);
  ce_thumbs_remove_param_box(key);
  lives_signal_handler_unblock(fxcombos[key],combo_fns[key]);
}


static void pin_toggled(LiVESToggleButton *t, livespointer pkey) {
  int key=LIVES_POINTER_TO_INT(pkey);
  boolean state=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"pinned"));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"pinned",LIVES_INT_TO_POINTER(!state));
}

#if LIVES_HAS_GRID_WIDGET

static void clip_area_toggled(LiVESToggleButton *t, livespointer parea) {
  int area=LIVES_POINTER_TO_INT(parea);
  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(rb_clip_areas[area]))) {
    mainw->active_sa_clips=area;
    ce_thumbs_highlight_current_clip();
  }
}

#endif

#define SPARE_CLIP_BOXES 100

void start_ce_thumb_mode(void) {
#if LIVES_HAS_GRID_WIDGET

  LiVESWidget *thumb_image=NULL;
  LiVESWidget *vbox,*vbox2,*vbox3;
  LiVESWidget *usibl=NULL,*sibl=NULL;
  LiVESWidget *hbox,*hbox2;
  LiVESWidget *tscroll,*cscroll;
  LiVESWidget *label;
  LiVESWidget *arrow;

  LiVESWidget *tgrid=lives_grid_new();

  LiVESWidget *align;

  LiVESPixbuf *thumbnail;

  LiVESList *cliplist=mainw->cliplist;
  LiVESList *fxlist=NULL;

  GSList *rb_fx_areas_group=NULL;
  GSList *rb_clip_areas_group=NULL;

  char filename[PATH_MAX];
  char *tmp;

  int width=CLIP_THUMB_WIDTH,height=CLIP_THUMB_HEIGHT;
  int modes=rte_getmodespk();
  int cpw;

  int count=-1,rcount=0;

  register int i,j;

  rte_keys_virtual=prefs->rte_keys_virtual;
  n_screen_areas=mainw->n_screen_areas;
  n_clip_boxes=lives_list_length(mainw->cliplist)+SPARE_CLIP_BOXES;

  next_screen_area=SCREEN_AREA_NONE;

  lives_grid_set_row_spacing(LIVES_GRID(tgrid),0);
  lives_grid_set_column_spacing(LIVES_GRID(tgrid),0);

  //lives_container_set_border_width (LIVES_CONTAINER (tgrid), width);

  // dual monitor mode, the gui monitor can show clip thumbnails

  top_hbox=lives_hbox_new(FALSE, 0);
  lives_widget_show(top_hbox);
  lives_box_pack_start(LIVES_BOX(mainw->vbox1), top_hbox, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) lives_widget_set_bg_color(top_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  // fx area
  vbox=lives_vbox_new(FALSE, widget_opts.packing_height);

  tscroll=lives_standard_scrolled_window_new(width,height,vbox);
  lives_box_pack_start(LIVES_BOX(top_hbox), tscroll, FALSE, TRUE, 0);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(tscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);
  lives_widget_set_hexpand(tscroll,FALSE);

  fxcombos=(LiVESWidget **)lives_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  pscrolls=(LiVESWidget **)lives_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  combo_entries=(LiVESWidget **)lives_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  key_checks=(LiVESWidget **)lives_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));

  rb_fx_areas=(LiVESWidget **)lives_malloc((n_screen_areas)*modes*sizeof(LiVESWidget *));
  rb_clip_areas=(LiVESWidget **)lives_malloc((n_screen_areas)*modes*sizeof(LiVESWidget *));

  clip_boxes=(LiVESWidget **)lives_malloc((n_clip_boxes)*modes*sizeof(LiVESWidget *));

  ch_fns=(ulong *)lives_malloc((rte_keys_virtual)*sizeof(ulong));
  combo_fns=(ulong *)lives_malloc((rte_keys_virtual)*sizeof(ulong));
  rb_clip_fns=(ulong *)lives_malloc((n_screen_areas)*sizeof(ulong));
  rb_fx_fns=(ulong *)lives_malloc((n_screen_areas)*sizeof(ulong));

  for (i=0; i<n_clip_boxes; i++) {
    clip_boxes[i]=NULL;
  }

  for (i=0; i<rte_keys_virtual; i++) {

    pscrolls[i]=NULL;

    fxlist=NULL;

    for (j=0; j<=rte_key_getmaxmode(i+1); j++) {
      fxlist=lives_list_append(fxlist,rte_keymode_get_filter_name(i+1,j));
    }

    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    tmp=lives_strdup_printf(_("Mapped to ctrl-%d"),i+1);
    key_checks[i]=lives_standard_check_button_new(NULL,FALSE,LIVES_BOX(hbox),tmp);
    lives_free(tmp);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_checks[i]),mainw->rte&(GU641<<i));
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(key_checks[i]),"active",
                                 LIVES_INT_TO_POINTER(lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(key_checks[i]))));

    ch_fns[i]=lives_signal_connect_after(LIVES_GUI_OBJECT(key_checks[i]), LIVES_WIDGET_TOGGLED_SIGNAL,
                                         LIVES_GUI_CALLBACK(rte_on_off_callback_hook),LIVES_INT_TO_POINTER(i+1));


    fxcombos[i]=lives_standard_combo_new(NULL,FALSE,fxlist,LIVES_BOX(hbox),NULL);

    if (fxlist!=NULL) {
      lives_list_free_strings(fxlist);
      lives_list_free(fxlist);
      lives_combo_set_active_index(LIVES_COMBO(fxcombos[i]),rte_key_getmode(i+1));
    } else {
      lives_widget_set_sensitive(key_checks[i],FALSE);
    }

    combo_entries[i] = lives_combo_get_entry(LIVES_COMBO(fxcombos[i]));

    lives_entry_set_editable(LIVES_ENTRY(combo_entries[i]), FALSE);

    combo_fns[i]=lives_signal_connect(LIVES_GUI_OBJECT(fxcombos[i]), LIVES_WIDGET_CHANGED_SIGNAL,
                                      LIVES_GUI_CALLBACK(ce_thumbs_fx_changed),LIVES_INT_TO_POINTER(i));

  }

  add_vsep_to_box(LIVES_BOX(top_hbox));

  // rhs vbox
  vbox2=lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(top_hbox), vbox2, TRUE, TRUE, 0);

  // rhs top hbox
  hbox2=lives_hbox_new(FALSE, widget_opts.packing_width);
  lives_box_pack_start(LIVES_BOX(vbox2), hbox2, TRUE, TRUE, 0);

  // vbox for arrows and areas
  vbox3=lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(hbox2), vbox3, FALSE, TRUE, 0);

  // add arrows
  hbox = lives_hbox_new(FALSE, 0);
  lives_widget_set_hexpand(hbox,FALSE);


  lives_box_pack_start(LIVES_BOX(vbox3), hbox, FALSE, TRUE, 0);
  arrow=lives_arrow_new(LIVES_ARROW_LEFT, LIVES_SHADOW_NONE);
  lives_box_pack_start(LIVES_BOX(hbox), arrow, FALSE, TRUE, 0);

  label=lives_standard_label_new(_("Effects"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);
  add_fill_to_box(LIVES_BOX(hbox));
  label=lives_standard_label_new(_("Clips"));
  lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

  arrow=lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_NONE);
  lives_box_pack_start(LIVES_BOX(hbox), arrow, FALSE, TRUE, 0);




  // screen areas
  vbox=lives_vbox_new(FALSE, widget_opts.packing_height);
  tscroll=lives_standard_scrolled_window_new(width,height,vbox);

  lives_box_pack_start(LIVES_BOX(vbox3), tscroll, FALSE, TRUE, 0);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(tscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);
  lives_widget_set_hexpand(tscroll,FALSE);


  for (i=0; i<n_screen_areas; i++) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    // radiobuttons for fx
    rb_fx_areas[i]=lives_standard_radio_button_new("",FALSE,rb_fx_areas_group,LIVES_BOX(hbox),
                   (tmp=lives_strdup_printf(_("Show / apply effects to %s\n"),
                                            mainw->screen_areas[i].name)));
    rb_fx_areas_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(rb_fx_areas[i]));
    lives_free(tmp);

    if (i!=SCREEN_AREA_FOREGROUND) lives_widget_set_sensitive(rb_fx_areas[i],FALSE);

    label=lives_standard_label_new(mainw->screen_areas[i].name);
    lives_box_pack_start(LIVES_BOX(hbox), label, FALSE, TRUE, 0);

    // radiobuttons for fx
    rb_clip_areas[i]=lives_standard_radio_button_new("",FALSE,rb_clip_areas_group,LIVES_BOX(hbox),
                     (tmp=lives_strdup_printf(_("Select clip for %s\n"),
                          mainw->screen_areas[i].name)));
    rb_clip_areas_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(rb_clip_areas[i]));
    lives_free(tmp);

    rb_clip_fns[i]=lives_signal_connect(LIVES_GUI_OBJECT(rb_clip_areas[i]), LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(clip_area_toggled),
                                        LIVES_INT_TO_POINTER(i));

  }

  add_vsep_to_box(LIVES_BOX(hbox2));

  cscroll=lives_standard_scrolled_window_new(width,height,tgrid);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(cscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);
  lives_box_pack_start(LIVES_BOX(hbox2), cscroll, TRUE, TRUE, 0);

  ////
  add_hsep_to_box(LIVES_BOX(vbox2));

  // insert a scrolled window for param boxes
  param_hbox = lives_hbox_new(FALSE, 0);

  tscroll=lives_standard_scrolled_window_new(width,height,param_hbox);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(tscroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_NEVER);

  lives_box_pack_start(LIVES_BOX(vbox2), tscroll, TRUE, TRUE, 0);

  lives_widget_hide(mainw->eventbox);
  lives_widget_hide(mainw->message_box);
  lives_widget_show_all(top_hbox);

  lives_widget_context_update(); // need size of cscroll to fit thumbs


  cpw=(lives_widget_get_allocation_width(tscroll)-widget_opts.border_width*2)/(width*1.5)-2;

  // add thumbs to grid

  while (cliplist!=NULL) {
    count++;

    i=LIVES_POINTER_TO_INT(cliplist->data);
    if (i==mainw->scrap_file||i==mainw->ascrap_file||
        (mainw->files[i]->clip_type!=CLIP_TYPE_DISK&&mainw->files[i]->clip_type!=CLIP_TYPE_FILE&&
         mainw->files[i]->clip_type!=CLIP_TYPE_YUV4MPEG&&mainw->files[i]->clip_type!=CLIP_TYPE_VIDEODEV)||
        mainw->files[i]->frames==0) {
      cliplist=cliplist->next;
      continue;
    }

    // make a small thumbnail, add it to the clips box
    thumbnail=make_thumb(NULL,i,width,height,mainw->files[i]->start,TRUE);

    clip_boxes[count]=lives_event_box_new();
    lives_widget_object_set_data(LIVES_WIDGET_OBJECT(clip_boxes[count]),"clipno",LIVES_INT_TO_POINTER(i));
    lives_widget_set_size_request(clip_boxes[count], width*1.5, height*1.5);

    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(clip_boxes[count], LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
      lives_widget_set_bg_color(clip_boxes[count], LIVES_WIDGET_STATE_PRELIGHT, &palette->menu_and_bars);
    }

    lives_widget_add_events(clip_boxes[count], LIVES_BUTTON_PRESS_MASK);

    align=lives_alignment_new(.5,.5,0.,0.);

    thumb_image=lives_image_new();
    lives_image_set_from_pixbuf(LIVES_IMAGE(thumb_image),thumbnail);
    if (thumbnail!=NULL) lives_object_unref(thumbnail);
    lives_container_add(LIVES_CONTAINER(clip_boxes[count]), align);

    if (rcount>0) {
      if (rcount==cpw-1) rcount=0;
      else {
        lives_grid_attach_next_to(LIVES_GRID(tgrid),clip_boxes[count],sibl,LIVES_POS_RIGHT,1,1);
        sibl=clip_boxes[count];
      }
    }

    if (rcount==0) {
      lives_grid_attach_next_to(LIVES_GRID(tgrid),clip_boxes[count],usibl,LIVES_POS_BOTTOM,1,1);
      sibl=usibl=clip_boxes[count];
    }

    lives_snprintf(filename,PATH_MAX,"%s",(tmp=lives_path_get_basename(mainw->files[i]->name)));
    lives_free(tmp);
    get_basename(filename);
    lives_widget_set_tooltip_text(clip_boxes[count], filename);

    //if (palette->style&STYLE_3) lives_widget_set_fg_color (label, LIVES_WIDGET_STATE_PRELIGHT, &palette->info_text);
    //if (palette->style&STYLE_4) lives_widget_set_fg_color (label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

    lives_container_add(LIVES_CONTAINER(align), thumb_image);

    rcount++;

    lives_signal_connect(LIVES_GUI_OBJECT(clip_boxes[count]), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(switch_clip_cb),
                         LIVES_INT_TO_POINTER(i));

    cliplist=cliplist->next;
  }

  if (prefs->open_maximised) {
    lives_window_maximize(LIVES_WINDOW(mainw->LiVES));
  }

  lives_widget_show_all(top_hbox);

  ce_thumbs_liberate_clip_area(mainw->num_tr_applied>0?SCREEN_AREA_BACKGROUND:SCREEN_AREA_FOREGROUND);
  ce_thumbs_set_clip_area();

  mainw->ce_thumbs=TRUE;

#endif
}


void end_ce_thumb_mode(void) {
  mainw->ce_thumbs=FALSE;
  ce_thumbs_remove_param_boxes(TRUE);
  lives_widget_destroy(top_hbox);
  lives_widget_show(mainw->eventbox);
  lives_widget_show(mainw->message_box);
  lives_free(fxcombos);
  lives_free(pscrolls);
  lives_free(combo_entries);
  lives_free(key_checks);
  lives_free(rb_fx_areas);
  lives_free(rb_clip_areas);
  lives_free(clip_boxes);
  lives_free(ch_fns);
  lives_free(rb_clip_fns);
  lives_free(rb_fx_fns);
}




void ce_thumbs_add_param_box(int key, boolean remove) {
  // when an effect with params is applied, show the parms in a box
  weed_plant_t *inst,*ninst;
  lives_rfx_t *rfx;

  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *pin_check;

  char *fname,*tmp,*tmp2;

  int mode=rte_key_getmode(key+1);
  int error;

  if (key>=rte_keys_virtual) return;

  pthread_mutex_lock(&mainw->gtk_mutex);

  if (remove) {
    // remove old boxes unless pinned
    ce_thumbs_remove_param_boxes(FALSE);
  }

  ninst=inst=rte_keymode_get_instance(key+1,mode);

  rfx=weed_to_rfx(inst,FALSE);
  rfx->min_frames=-1;

  do {
    weed_instance_ref(ninst);
  } while (weed_plant_has_leaf(ninst,"host_next_instance")&&(ninst=weed_get_plantptr_value(ninst,"host_next_instance",&error))!=NULL);


  // here we just check if we have any params to display
  if (!make_param_box(NULL,rfx)) {
    rfx_free(rfx);
    lives_free(rfx);
    return;
  }


  vbox = lives_vbox_new(FALSE, 0);

  pscrolls[key]=lives_standard_scrolled_window_new(-1,-1,vbox);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(pscrolls[key]), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);
  lives_widget_set_hexpand(pscrolls[key],FALSE);

  lives_box_pack_start(LIVES_BOX(param_hbox), pscrolls[key], TRUE, TRUE, 0);

  fname=weed_instance_get_filter_name(inst,TRUE);
  label=lives_standard_label_new(fname);
  lives_free(fname);

  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);

  add_fill_to_box(LIVES_BOX(hbox));
  lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_height);
  add_fill_to_box(LIVES_BOX(hbox));

  /* TRANSLATORS - "pin" as in "pinned to window" */
  pin_check=lives_standard_check_button_new((tmp=lives_strdup(_("_Pin"))),TRUE,LIVES_BOX(hbox),
            (tmp2=lives_strdup(_("Pin the parameter box to the window"))));
  lives_free(tmp);
  lives_free(tmp2);

  lives_signal_connect_after(LIVES_GUI_OBJECT(pin_check), LIVES_WIDGET_TOGGLED_SIGNAL,
                             LIVES_GUI_CALLBACK(pin_toggled),LIVES_INT_TO_POINTER(key));


  on_fx_pre_activate(rfx,1,vbox);

  // record the key so we know whose parameters to record later
  weed_set_int_value((weed_plant_t *)rfx->source,"host_key",key);

  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"pinned",LIVES_INT_TO_POINTER(FALSE));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"update",LIVES_INT_TO_POINTER(FALSE));
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"rfx",rfx);
  lives_widget_show_all(param_hbox);
  pthread_mutex_unlock(&mainw->gtk_mutex);
}


static void ce_thumbs_remove_param_box(int key) {
  // remove a single param box from the param_hbox
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;
  if (pscrolls[key]==NULL) return;
  rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"rfx");
  on_paramwindow_cancel_clicked(NULL,rfx); // free rfx and unref the inst (must be done before destroying the pscrolls[key]
  lives_widget_destroy(pscrolls[key]);
  pscrolls[key]=NULL;
  lives_widget_queue_draw(param_hbox);
}


static void ce_thumbs_remove_param_boxes(boolean remove_pinned) {
  // remove all param boxes, (except any which are "pinned")
  register int i;
  for (i=0; i<rte_keys_virtual; i++) {
    if (pscrolls[i]!=NULL) {
      if (remove_pinned||!LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[i]),"pinned")))
        ce_thumbs_remove_param_box(i);
    }
  }
}



void ce_thumbs_register_rfx_change(int key, int mode) {
  // register a param box to be updated visually, from an asynchronous source - either from a A->V data connection or from osc
  if (key>=rte_keys_virtual||pscrolls[key]==NULL) return;
  lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"update",LIVES_INT_TO_POINTER(TRUE));
}


void ce_thumbs_apply_rfx_changes(void) {
  // apply asynch updates
  lives_rfx_t *rfx;
  register int i;

  for (i=0; i<rte_keys_virtual; i++) {
    if (pscrolls[i]!=NULL) {
      if (LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[i]),"update"))) {
        lives_widget_object_set_data(LIVES_WIDGET_OBJECT(pscrolls[i]),"update",LIVES_INT_TO_POINTER(FALSE));
        rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[i]),"rfx");
        update_visual_params(rfx,FALSE);
      }
    }
  }
}


void ce_thumbs_update_params(int key, int i, LiVESList *list) {
  // called only from weed_set_blend_factor() and from setting param in rte_window
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;

  if (pscrolls[key]!=NULL) {
    rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"rfx");
    mainw->block_param_updates=TRUE;
    set_param_from_list(list,&rfx->params[key],0,TRUE,TRUE);
    mainw->block_param_updates=FALSE;
  }
}


void ce_thumbs_update_visual_params(int key) {
  // param change in rte_window - set params box here
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;

  if (pscrolls[key]!=NULL) {
    rfx=(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"rfx");
    update_visual_params(rfx,FALSE);
  }
}


void ce_thumbs_check_for_rte(lives_rfx_t *rfx, lives_rfx_t *rte_rfx, int key) {
  // param change in ce_thumbs, update rte_window
  register int i;
  for (i=0; i<rte_keys_virtual; i++) {
    if (pscrolls[i]!=NULL&&i==key&&rfx==(lives_rfx_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(pscrolls[key]),"rfx")) {
      update_visual_params(rte_rfx,FALSE);
      break;
    }
  }
}


void ce_thumbs_reset_combo(int key) {
  // called from rte_window when the mapping is changed

  LiVESList *fxlist=NULL;
  int mode;
  register int j;

  if (key>=rte_keys_virtual) return;
  for (j=0; j<=rte_key_getmaxmode(key+1); j++) {
    fxlist=lives_list_append(fxlist,rte_keymode_get_filter_name(key+1,j));
  }
  lives_combo_populate(LIVES_COMBO(fxcombos[key]),fxlist);
  if (fxlist!=NULL) {
    lives_widget_set_sensitive(key_checks[key],TRUE);
    lives_list_free_strings(fxlist);
    lives_list_free(fxlist);
    mode=rte_key_getmode(key+1);
    ce_thumbs_set_mode_combo(key,mode);
    if (rte_keymode_get_instance(key+1,mode)!=NULL) ce_thumbs_add_param_box(key,TRUE);
  } else {
    lives_widget_set_sensitive(key_checks[key],FALSE);
    lives_combo_set_active_string(LIVES_COMBO(fxcombos[key]),"");
  }

}


void ce_thumbs_reset_combos(void) {
  // called from rte_window when the mapping is cleared
  register int i;
  for (i=0; i<rte_keys_virtual; i++) {
    ce_thumbs_reset_combo(i);
  }
}


void ce_thumbs_set_clip_area(void) {
  register int i;
  for (i=0; i<n_screen_areas; i++) lives_signal_handler_block(rb_clip_areas[i],rb_clip_fns[i]);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_clip_areas[mainw->active_sa_clips]), TRUE);
  for (i=0; i<n_screen_areas; i++) lives_signal_handler_unblock(rb_clip_areas[i],rb_clip_fns[i]);
  ce_thumbs_highlight_current_clip();
}


void ce_thumbs_set_fx_area(int area) {
  //register int i;
  //for (i=0;i<n_screen_areas;i++) lives_signal_handler_block(rb_fx_areas[i],rb_fx_fns[i]);
  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(rb_fx_areas[area]), TRUE);
  //for (i=0;i<n_screen_areas;i++) lives_signal_handler_unblock(rb_fx_areas[i],rb_fx_fns[i]);
  mainw->active_sa_fx=area;
}


void ce_thumbs_update_current_clip(void) {
  mainw->ce_upd_clip=TRUE;
}


void ce_thumbs_highlight_current_clip(void) {
  // unprelight all clip boxes, prelight current clip (fg or bg)
  boolean match=FALSE;
  int clipno;
  register int i;

  for (i=0; i<n_clip_boxes; i++) {
    if (clip_boxes[i]==NULL) break;
    if (!match) {
      clipno=LIVES_POINTER_TO_INT(lives_widget_object_get_data(LIVES_WIDGET_OBJECT(clip_boxes[i]),"clipno"));
      switch (mainw->active_sa_clips) {
      case SCREEN_AREA_FOREGROUND:
        if (clipno==mainw->current_file) match=TRUE;
        break;
      case SCREEN_AREA_BACKGROUND:
        if (clipno==mainw->blend_file) match=TRUE;
        if (mainw->blend_file==-1&&clipno==mainw->current_file) match=TRUE;
        break;
      default:
        break;
      }
      if (match) {
        lives_widget_set_state(clip_boxes[i],LIVES_WIDGET_STATE_PRELIGHT);
        continue;
      }
    }
    lives_widget_set_state(clip_boxes[i],LIVES_WIDGET_STATE_NORMAL);
  }

}


void ce_thumbs_liberate_clip_area(int area) {
  lives_widget_set_sensitive(rb_clip_areas[area],TRUE);
  ce_thumbs_set_clip_area();
}


void ce_thumbs_liberate_clip_area_register(int area) {
  next_screen_area=area;
}


void ce_thumbs_apply_liberation(void) {
  if (next_screen_area!=SCREEN_AREA_NONE)
    ce_thumbs_liberate_clip_area(next_screen_area);
  next_screen_area=SCREEN_AREA_NONE;
}

