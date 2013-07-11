// ce_thumbs.c
// LiVES
// (c) G. Finch 2013 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// clip thumbnails window for dual head mode


// TODO - drag fx order :  check for data conx

// TODO - indicate fg and bg clips

// TODO - buttons for some keys

// TODO - exp text for fx

// TODO - display screen mapping areas


#include "support.h"
#include "main.h"
#include "effects-weed.h"
#include "effects.h"
#include "paramwindow.h"
#include "ce_thumbs.h"

static LiVESWidget **fxcombos;
static LiVESWidget **pscrolls;
static LiVESWidget **combo_entries;
static LiVESWidget **key_checks;
static LiVESWidget *param_hbox;
static LiVESWidget *top_hbox;
static gulong *ch_fns;
static gulong *combo_fns;

static int rte_keys_virtual;

static void ce_thumbs_remove_param_boxes(boolean remove_pinned);
static void ce_thumbs_remove_param_box(int key);

#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
static boolean switch_clip_cb (LiVESWidget *eventbox, LiVESXEventButton *event, gpointer user_data) {
  int i=GPOINTER_TO_INT(user_data);
  if (mainw->playing_file==-1) return FALSE;
  switch_clip (0,i);
  return FALSE;
}

static void ce_thumbs_fx_changed (GtkComboBox *combo, gpointer user_data) {
  // callback after user switches fx via combo 
  int key=LIVES_POINTER_TO_INT(user_data);
  int mode,cmode;

  if ((mode=lives_combo_get_active(combo))==-1) return; // -1 is returned after we set our own text (without the type)
  cmode=rte_key_getmode(key+1);

  if (cmode==mode) return;

  lives_widget_grab_focus (combo_entries[key]);

  rte_key_setmode(key+1,mode);
}
#endif


void ce_thumbs_set_keych (int key, boolean on) {
  // set key check from other source
  if (key>=rte_keys_virtual) return;
  g_signal_handler_block(key_checks[key],ch_fns[key]);
  lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON(key_checks[key]),on);
  if (!on&&pscrolls[key]!=NULL) ce_thumbs_remove_param_box(key);
  g_signal_handler_unblock(key_checks[key],ch_fns[key]);
}


void ce_thumbs_set_mode_combo (int key, int mode) {
  // set combo from other source : need to add params after
  if (key>=rte_keys_virtual) return;
  if (mode<0) return;
  g_signal_handler_block(fxcombos[key],combo_fns[key]);
  lives_combo_set_active_index (LIVES_COMBO (fxcombos[key]),mode);
  ce_thumbs_remove_param_box(key);
  g_signal_handler_unblock(fxcombos[key],combo_fns[key]);
}


static void pin_toggled (LiVESToggleButton *t, livespointer pkey) {
  int key=LIVES_POINTER_TO_INT(pkey);
  boolean state=LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(pscrolls[key]),"pinned"));
  g_object_set_data (G_OBJECT (pscrolls[key]),"pinned",LIVES_INT_TO_POINTER (!state));
}


void start_ce_thumb_mode(void) {
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget

  LiVESWidget *thumb_image=NULL;
  LiVESWidget *vbox,*vbox2;
  LiVESWidget *eventbox;
  LiVESWidget *usibl=NULL,*sibl=NULL;
  LiVESWidget *hbox;
  LiVESWidget *tscroll;

  LiVESWidget *tgrid=lives_grid_new();

  LiVESPixbuf *thumbnail;

  GList *cliplist=mainw->cliplist;

  GList *fxlist=NULL;

  char filename[PATH_MAX];
  char *tmp;

  int width=CLIP_THUMB_WIDTH,height=CLIP_THUMB_HEIGHT;
  int modes=rte_getmodespk();
  int cpw,scr_width;

  int count=0;

  register int i,j;

  rte_keys_virtual=prefs->rte_keys_virtual;

  lives_grid_set_row_spacing (LIVES_GRID(tgrid),width>>1);
  lives_grid_set_column_spacing (LIVES_GRID(tgrid),height>>1);

  lives_container_set_border_width (LIVES_CONTAINER (tgrid), width);

  // dual monitor mode, the gui monitor can show clip thumbnails
  lives_widget_hide(mainw->eventbox);

  top_hbox=lives_hbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (mainw->vbox1), top_hbox, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) lives_widget_set_bg_color (top_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  // fx area
  vbox=lives_vbox_new (FALSE, widget_opts.packing_height);
  
  tscroll=lives_standard_scrolled_window_new(width*2,height,vbox);
  lives_box_pack_start (LIVES_BOX (top_hbox), tscroll, FALSE, TRUE, 0);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (tscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);

  fxcombos=(LiVESWidget **)g_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  pscrolls=(LiVESWidget **)g_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  combo_entries=(LiVESWidget **)g_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  key_checks=(LiVESWidget **)g_malloc((rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  ch_fns=(gulong *)g_malloc((rte_keys_virtual)*sizeof(gulong));
  combo_fns=(gulong *)g_malloc((rte_keys_virtual)*sizeof(gulong));

  for (i=0;i<rte_keys_virtual;i++) {

    pscrolls[i]=NULL;

    fxlist=NULL;

    for (j=0;j<=rte_key_getmaxmode(i+1);j++) {
      fxlist=g_list_append(fxlist,rte_keymode_get_filter_name(i+1,j));
    }

    hbox = lives_hbox_new (FALSE, 0);
    lives_box_pack_start (LIVES_BOX (vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    tmp=g_strdup_printf(_("Mapped to ctrl-%d"),i+1);
    key_checks[i]=lives_standard_check_button_new(NULL,FALSE,LIVES_BOX(hbox),tmp);
    g_free(tmp);
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_checks[i]),mainw->rte&(GU641<<i));

    ch_fns[i]=g_signal_connect_after (GTK_OBJECT (key_checks[i]), "toggled",
				      G_CALLBACK (rte_on_off_callback_hook),GINT_TO_POINTER (i+1));


    fxcombos[i]=lives_standard_combo_new(NULL,FALSE,fxlist,LIVES_BOX(hbox),NULL);

    if (fxlist!=NULL) {
      g_list_free_strings(fxlist);
      g_list_free(fxlist);
      lives_combo_set_active_index (LIVES_COMBO (fxcombos[i]),rte_key_getmode(i+1));
    }
    else {
      lives_widget_set_sensitive(key_checks[i],FALSE);
    }

    combo_entries[i] = lives_combo_get_entry(LIVES_COMBO(fxcombos[i]));
 
    lives_entry_set_editable (LIVES_ENTRY (combo_entries[i]), FALSE);
      
    combo_fns[i]=g_signal_connect(GTK_OBJECT (fxcombos[i]), "changed",
				  G_CALLBACK (ce_thumbs_fx_changed),GINT_TO_POINTER(i));

  }

  add_vsep_to_box(LIVES_BOX(top_hbox));

  vbox2=lives_vbox_new (FALSE, 0);
  lives_box_pack_start (LIVES_BOX (top_hbox), vbox2, TRUE, TRUE, 0);

  // insert a scrolled window for thumbs
  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
  }

  cpw=scr_width*2/width/3-2;
  lives_widget_set_size_request(vbox2,width*cpw,-1);

  tscroll=lives_standard_scrolled_window_new(width*cpw,height,tgrid);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (tscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);

  lives_box_pack_start (LIVES_BOX (vbox2), tscroll, TRUE, TRUE, 0);

  // add thumbs to grid

  while (cliplist!=NULL) {
    i=GPOINTER_TO_INT(cliplist->data);
    if (i==mainw->scrap_file||i==mainw->ascrap_file||
	(mainw->files[i]->clip_type!=CLIP_TYPE_DISK&&mainw->files[i]->clip_type!=CLIP_TYPE_FILE&&
	 mainw->files[i]->clip_type!=CLIP_TYPE_YUV4MPEG&&mainw->files[i]->clip_type!=CLIP_TYPE_VIDEODEV)||
	mainw->files[i]->frames==0) {
      cliplist=cliplist->next;
      continue;
    }

    // make a small thumbnail, add it to the clips box
    thumbnail=make_thumb(NULL,i,width,height,mainw->files[i]->start,TRUE);

    eventbox=lives_event_box_new();

    if (palette->style&STYLE_4) {
      lives_widget_set_bg_color(eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    gtk_widget_add_events (eventbox, GDK_BUTTON_PRESS_MASK);

    vbox = lives_vbox_new (FALSE, 6.*widget_opts.scale);

    thumb_image=lives_image_new();
    lives_image_set_from_pixbuf(LIVES_IMAGE(thumb_image),thumbnail);
    if (thumbnail!=NULL) lives_object_unref(thumbnail);
    lives_container_add (LIVES_CONTAINER (eventbox), vbox);

    if (count>0) {
      if (count==cpw-1) count=0;
      else {
	lives_grid_attach_next_to(LIVES_GRID(tgrid),eventbox,sibl,LIVES_POS_RIGHT,1,1);
	sibl=eventbox;
      }
    }

    if (count==0) {
      lives_grid_attach_next_to(LIVES_GRID(tgrid),eventbox,usibl,LIVES_POS_BOTTOM,1,1);
      sibl=usibl=eventbox;
    }

    g_snprintf (filename,PATH_MAX,"%s",(tmp=g_path_get_basename(mainw->files[i]->name)));
    g_free(tmp);
    get_basename(filename);
    lives_widget_set_tooltip_text(eventbox, filename);

    //if (palette->style&STYLE_3) lives_widget_set_fg_color (label, LIVES_WIDGET_STATE_PRELIGHT, &palette->info_text);
    //if (palette->style&STYLE_4) lives_widget_set_fg_color (label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

    lives_box_pack_start (LIVES_BOX (vbox), thumb_image, FALSE, FALSE, 0);
      
    count++;
      
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (switch_clip_cb),
		      GINT_TO_POINTER(i));
      
    cliplist=cliplist->next;
  }

  add_hsep_to_box(LIVES_BOX(vbox2));

  // insert a scrolled window for param boxes
  param_hbox = lives_hbox_new (FALSE, 0);

  tscroll=lives_standard_scrolled_window_new(width,height,param_hbox);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (tscroll), LIVES_POLICY_AUTOMATIC, LIVES_POLICY_NEVER);

  lives_box_pack_start (LIVES_BOX (vbox2), tscroll, TRUE, TRUE, 0);

  lives_widget_show_all(top_hbox);

  mainw->ce_thumbs=TRUE;

#endif
}


void end_ce_thumb_mode(void) {
  ce_thumbs_remove_param_boxes(TRUE);
  lives_widget_destroy(top_hbox);
  lives_widget_show(mainw->eventbox);
  g_free(fxcombos);
  g_free(pscrolls);
  g_free(combo_entries);
  g_free(key_checks);
  g_free(ch_fns);
  g_free(combo_fns);
  mainw->ce_thumbs=FALSE;
}




void ce_thumbs_add_param_box(int key) {
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

  // remove old boxes unless pinned
  ce_thumbs_remove_param_boxes(FALSE);

  ninst=inst=rte_keymode_get_instance(key+1,mode);

  do {
    weed_instance_ref(ninst);
  } while (weed_plant_has_leaf(ninst,"host_next_instance")&&(ninst=weed_get_plantptr_value(ninst,"host_next_instance",&error))!=NULL);


  rfx=weed_to_rfx(inst,FALSE);
  rfx->min_frames=-1;

  vbox = lives_vbox_new (FALSE, 0);

  pscrolls[key]=lives_standard_scrolled_window_new(-1,-1,vbox);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (pscrolls[key]), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);

  lives_box_pack_start (LIVES_BOX (param_hbox), pscrolls[key], TRUE, TRUE, 0);

  fname=weed_instance_get_filter_name(inst,TRUE);
  label=lives_standard_label_new(fname);
  g_free(fname);

  hbox = lives_hbox_new (FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox),hbox,FALSE,FALSE,widget_opts.packing_height);

  add_fill_to_box(LIVES_BOX(hbox));
  lives_box_pack_start(LIVES_BOX(hbox),label,FALSE,FALSE,widget_opts.packing_height);
  add_fill_to_box(LIVES_BOX(hbox));

  /* TRANSLATORS - "pin" as in "pinned to window" */
  pin_check=lives_standard_check_button_new((tmp=g_strdup(_("_Pin"))),TRUE,LIVES_BOX(hbox),(tmp2=g_strdup(_("Pin the parameter box to the window"))));
  g_free(tmp); g_free(tmp2);

  g_signal_connect_after (GTK_OBJECT (pin_check), "toggled",
			  G_CALLBACK (pin_toggled),LIVES_INT_TO_POINTER (key));


  on_fx_pre_activate(rfx,1,vbox);

  // record the key so we know whose parameters to record later
  weed_set_int_value((weed_plant_t *)rfx->source,"host_hotkey",key);

  g_object_set_data (G_OBJECT (pscrolls[key]),"pinned",LIVES_INT_TO_POINTER (FALSE));
  g_object_set_data (G_OBJECT (pscrolls[key]),"update",LIVES_INT_TO_POINTER (FALSE));
  g_object_set_data (G_OBJECT (pscrolls[key]),"rfx",rfx);
  lives_widget_show_all(param_hbox);
}


static void ce_thumbs_remove_param_box(int key) {
  // remove a single param box from the param_hbox
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;
  if (pscrolls[key]==NULL) return;
  rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(pscrolls[key]),"rfx");
  on_paramwindow_cancel_clicked(NULL,rfx); // free rfx and unref the inst
  lives_widget_destroy(pscrolls[key]);
  pscrolls[key]=NULL;
  lives_widget_queue_draw(param_hbox);
}


static void ce_thumbs_remove_param_boxes(boolean remove_pinned) {
  // remove all param boxes, (except any which are "pinned")
  register int i;
  for (i=0;i<rte_keys_virtual;i++) {
    if (pscrolls[i]!=NULL) {
      if (remove_pinned||!LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(pscrolls[i]),"pinned"))) 
	ce_thumbs_remove_param_box(i);
    }
  }
}



void ce_thumbs_register_rfx_change(int key, int mode) {
  // register a param box to be updated visually, from an asynchronous source - either from a A->V data connection or from osc
  if (key>=rte_keys_virtual) return;
  g_object_set_data (G_OBJECT (pscrolls[key]),"update",LIVES_INT_TO_POINTER (TRUE));
}


void ce_thumbs_apply_rfx_changes(void) {
  // apply asynch updates
  lives_rfx_t *rfx;
  register int i;

  for (i=0;i<rte_keys_virtual;i++) {
    if (pscrolls[i]!=NULL) {
      if (!LIVES_POINTER_TO_INT(g_object_get_data(G_OBJECT(pscrolls[i]),"update"))) 
	g_object_set_data (G_OBJECT (pscrolls[i]),"update",LIVES_INT_TO_POINTER (FALSE));
	rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(pscrolls[i]),"rfx");
	update_visual_params(rfx,FALSE);
    }
  }
}


void ce_thumbs_update_params (int key, int i, GList *list) {
  // called only from weed_set_blend_factor() and from setting param in rte_window
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;

  if (pscrolls[key]!=NULL) {
    rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(pscrolls[key]),"rfx");
    mainw->block_param_updates=TRUE;
    set_param_from_list(list,&rfx->params[key],0,TRUE,TRUE);
    mainw->block_param_updates=FALSE;
  }
}


void ce_thumbs_update_visual_params (int key) {
  // param change in rte_window - set params box here
  lives_rfx_t *rfx;
  if (key>=rte_keys_virtual) return;

  if (pscrolls[key]!=NULL) {
    rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(pscrolls[key]),"rfx");
    update_visual_params(rfx,FALSE);
  }
}


void ce_thumbs_check_for_rte(lives_rfx_t *rfx, lives_rfx_t *rte_rfx, int key) {
  // param change in ce_thumbs, update rte_window
  register int i;
  for (i=0;i<rte_keys_virtual;i++) {
    if (pscrolls[i]!=NULL&&i==key&&rfx==(lives_rfx_t *)g_object_get_data(G_OBJECT(pscrolls[key]),"rfx")) {
      update_visual_params(rte_rfx,FALSE);
      break;
    }
  }
}


void ce_thumbs_reset_combo(int key) {
  // called from rte_window when the mapping is changed

  GList *fxlist=NULL;
  int mode;
  register int j;

  if (key>=rte_keys_virtual) return;
  for (j=0;j<=rte_key_getmaxmode(key+1);j++) {
    fxlist=g_list_append(fxlist,rte_keymode_get_filter_name(key+1,j));
  }
  lives_combo_populate(LIVES_COMBO(fxcombos[key]),fxlist);
  if (fxlist!=NULL) {
    lives_widget_set_sensitive(key_checks[key],TRUE);
    g_list_free_strings(fxlist);
    g_list_free(fxlist);
    mode=rte_key_getmode(key+1);
    ce_thumbs_set_mode_combo(key,mode);
    if (rte_keymode_get_instance(key+1,mode)!=NULL) ce_thumbs_add_param_box(key);
  }
  else {
    lives_widget_set_sensitive(key_checks[key],FALSE);
    lives_combo_set_active_string(LIVES_COMBO(fxcombos[key]),"");
  }

}


void ce_thumbs_reset_combos(void) {
  // called from rte_window when the mapping is cleared
  register int i;
  for (i=0;i<rte_keys_virtual;i++) {
    ce_thumbs_reset_combo(i);
  }
}
