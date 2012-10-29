// paramspecial.c
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// dynamic window generation from parameter arrays :-)
// special widgets

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "resample.h"
#include "effects.h"
#include "support.h"
#include "paramwindow.h"

static lives_special_aspect_t aspect;
static lives_special_framedraw_rect_t framedraw;
static GList *fileread;
static GList *passwd_widgets;

// TODO - rewrite all of this more sensibly


void 
init_special (void) {
  aspect.width_param=aspect.height_param=-1;
  aspect.width_widget=aspect.height_widget=NULL;
  framedraw.type=FD_NONE;
  framedraw.xstart_param=framedraw.ystart_param=framedraw.xend_param=framedraw.yend_param=-1;
  framedraw.extra_params=NULL;
  framedraw.num_extra=0;
  framedraw.xstart_widget=framedraw.ystart_widget=framedraw.xend_widget=framedraw.yend_widget=NULL;
  framedraw.added=FALSE;
  mergealign.start_param=mergealign.end_param=-1;
  mergealign.start_widget=mergealign.end_widget=NULL;
  passwd_widgets=NULL;
  fileread=NULL;
}


gint 
add_to_special (const gchar *sp_string, lives_rfx_t *rfx) {
  gchar **array=g_strsplit (sp_string,"|",-1);
  int num_widgets=get_token_count(sp_string,'|')-2;
  int i,pnum;
  int *ign;

  weed_plant_t *param,*paramtmpl,*init_event=NULL;
  int num_in_tracks=0,error;

  void **pchains=NULL,*pchange;

  // TODO - make sure only one of each of these

  // returns extra width in pixels
  gint extra_width=0;

  if (!strcmp (array[0],"aspect")) {
    // aspect button
    if (fx_dialog[1]!=NULL) {
      g_strfreev(array);
      return extra_width;
    }
    aspect.width_param=atoi (array[1]);
    aspect.height_param=atoi (array[2]);
  }
  else if (!strcmp (array[0],"mergealign")) {
    // align start/end
    if (fx_dialog[1]!=NULL) {
      g_strfreev(array);
      return extra_width;
    }
    mergealign.start_param=atoi (array[1]);
    mergealign.end_param=atoi (array[2]);
    mergealign.rfx=rfx;
  }
  else if (!strcmp (array[0],"framedraw")) {
    gint stdwidgets=0;
    if (fx_dialog[1]!=NULL) {
      g_strfreev(array);
      return extra_width;
    }
    framedraw.rfx=rfx;
    if (!strcmp (array[1],"rectdemask")) {
      framedraw.type=FD_RECT_DEMASK;
      framedraw.xstart_param=atoi (array[2]);
      framedraw.ystart_param=atoi (array[3]);
      framedraw.xend_param=atoi (array[4]);
      framedraw.yend_param=atoi (array[5]);
      stdwidgets=4;
    }
    else if (!strcmp (array[1],"multrect")) {
      framedraw.type=FD_RECT_MULTRECT;
      framedraw.xstart_param=atoi (array[2]);
      framedraw.ystart_param=atoi (array[3]);
      framedraw.xend_param=atoi (array[4]);
      framedraw.yend_param=atoi (array[5]);
      stdwidgets=4;
    }
    else if (!strcmp (array[1],"singlepoint")) {
      framedraw.type=FD_SINGLEPOINT;
      framedraw.xstart_param=framedraw.xend_param=atoi (array[2]);
      framedraw.ystart_param=framedraw.yend_param=atoi (array[3]);
      stdwidgets=2;
    }

    if (num_widgets>stdwidgets) framedraw.extra_params=
				  (gint *)g_malloc(((framedraw.num_extra=(num_widgets-stdwidgets)))*sizint);
    if (rfx->status==RFX_STATUS_WEED&&mainw->multitrack!=NULL&&(init_event=mainw->multitrack->init_event)!=NULL) {
      num_in_tracks=weed_leaf_num_elements(init_event,"in_tracks");
      pchains=weed_get_voidptr_array(init_event,"in_parameters",&error);
    }
    ign=(int *)g_malloc(num_in_tracks*sizint);
    for (i=0;i<num_in_tracks;i++) ign[i]=WEED_FALSE;
    for (i=0;i<num_widgets;i++) {
      pnum=atoi(array[i+2]);
      if (rfx->status==RFX_STATUS_WEED) {
	if (mainw->multitrack!=NULL) {
	  // TODO - check rfx->params[pnum].multi
	  if ((rfx->params[pnum].hidden&HIDDEN_MULTI)==HIDDEN_MULTI) {
	    if (mainw->multitrack->track_index!=-1) {
	      rfx->params[pnum].hidden^=HIDDEN_MULTI; // multivalues allowed
	    }
	    else {
	      rfx->params[pnum].hidden|=HIDDEN_MULTI; // multivalues hidden
	    }
	  }
	}
	if (init_event!=NULL) {
	  param=weed_inst_in_param((weed_plant_t *)rfx->source,pnum,FALSE);
	  paramtmpl=weed_get_plantptr_value(param,"template",&error);
	  pchange=pchains[pnum];
	  fill_param_vals_to ((weed_plant_t *)pchange,paramtmpl,num_in_tracks-1);
	  weed_set_boolean_array((weed_plant_t *)pchange,"ignore",num_in_tracks,ign);
	}
      }
      if (i>=stdwidgets) framedraw.extra_params[i-stdwidgets]=pnum;
    }
    if (rfx->status==RFX_STATUS_WEED) {
      weed_free(pchains);
    }
    if (mainw->multitrack==NULL) extra_width=RFX_EXTRA_WIDTH;
    else {
      mainw->multitrack->framedraw=&framedraw;
      gtk_widget_modify_bg (mainw->multitrack->fd_frame, GTK_STATE_NORMAL, &palette->light_red);
    }
    g_free(ign);
  }
  else if (!strcmp (array[0],"fileread")) {
    fileread=g_list_append(fileread,GINT_TO_POINTER(atoi(array[1])));
  }
  else if (!strcmp (array[0],"password")) {
    int idx=atoi(array[1]);
    passwd_widgets=g_list_append(passwd_widgets,GINT_TO_POINTER(idx));

    // ensure we get an entry and not a text_view
    if ((gint)rfx->params[idx].max>RFX_TEXT_MAGIC) rfx->params[idx].max=(gdouble)RFX_TEXT_MAGIC;
  }

  g_strfreev (array);
  return extra_width;
}


void fd_tweak(lives_rfx_t *rfx) {
  if (rfx->props&RFX_PROPS_MAY_RESIZE) {
    if (framedraw.type!=FD_NONE) {
      // for effects which can resize, and have a special framedraw, we will use original sized image
      gtk_widget_hide(mainw->framedraw_preview);
      gtk_widget_set_sensitive(mainw->framedraw_spinbutton,TRUE);
      gtk_widget_set_sensitive(mainw->framedraw_scale,TRUE);
    }
  }
}


void fd_connect_spinbutton(lives_rfx_t *rfx) {
  framedraw_connect_spinbutton(&framedraw,rfx);
}


static void passwd_toggle_vis(GtkToggleButton *b, gpointer entry) {
  gtk_entry_set_visibility(GTK_ENTRY(entry),gtk_toggle_button_get_active(b));
}


void check_for_special (lives_param_t *param, gint num, GtkBox *pbox, lives_rfx_t *rfx) {
  GtkWidget *checkbutton;
  GtkWidget *hbox;
  GtkWidget *eventbox;
  GtkWidget *box;
  GtkWidget *buttond;
  GtkWidget *label;
  GList *slist;
  // check if this parameter is part of a special window
  // as we are drawing the paramwindow

  if (num==framedraw.xstart_param) {
    framedraw.xstart_widget=param->widgets[0];
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),0.);
    g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
  }
  if (num==framedraw.ystart_param) {
    framedraw.ystart_widget=param->widgets[0];
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),0.);
    g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
  }
  if (mainw->current_file>-1) {
    if (num==framedraw.xend_param) {
      framedraw.xend_widget=param->widgets[0];
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->hsize);
      g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
    }
    if (num==framedraw.yend_param) {
      framedraw.yend_widget=param->widgets[0];
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->vsize);
      g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
  }
    if (framedraw.xstart_widget!=NULL&&framedraw.ystart_widget!=NULL&&framedraw.xend_widget!=NULL&&framedraw.yend_widget!=NULL&&!framedraw.added) {
      if (mainw->multitrack==NULL) {
	framedraw_connect(&framedraw,cfile->hsize,cfile->vsize,rfx); // turn passive preview->active
	framedraw_add_reset(GTK_VBOX(GTK_WIDGET(pbox)),&framedraw);
      }
      else {
	mainw->framedraw=mainw->image274;
      }
      framedraw.added=TRUE;
    }
    
    
    if (num==aspect.width_param) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->hsize);
      aspect.width_func=g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed",
						G_CALLBACK (after_aspect_width_changed),
						NULL);
      aspect.width_widget=param->widgets[0];
    }
    if (num==aspect.height_param) {
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->vsize);
      aspect.height_func=g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed",
						 G_CALLBACK (after_aspect_height_changed),
						 NULL);
      
      box = gtk_hbox_new (FALSE, 10);
      gtk_box_pack_start (GTK_BOX (GTK_WIDGET (pbox)), box, TRUE, FALSE, 0);
      
      
      
      
      checkbutton = gtk_check_button_new ();
      gtk_widget_set_tooltip_text( checkbutton, (_("Maintain aspect ratio of original frame")));
      eventbox=gtk_event_box_new();
      lives_tooltips_copy(eventbox,checkbutton);
      label=gtk_label_new_with_mnemonic (_("Maintain _Aspect Ratio"));
      
      
      gtk_container_add(GTK_CONTAINER(eventbox),label);
      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (label_act_toggle),
			checkbutton);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
      
      gtk_label_set_mnemonic_widget (GTK_LABEL (label),checkbutton);
      
      hbox = gtk_hbox_new (FALSE, 10);
      gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, 10);
      add_fill_to_box(GTK_BOX(hbox));
      gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, 10);
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
      add_fill_to_box(GTK_BOX(hbox));
      GTK_WIDGET_SET_FLAGS (checkbutton, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton), TRUE);
      aspect.checkbutton=checkbutton;
      aspect.height_widget=param->widgets[0];
    }
    if (num==mergealign.start_param) mergealign.start_widget=param->widgets[0];
    if (num==mergealign.end_param) mergealign.end_widget=param->widgets[0];
  }

  slist=fileread;
  while (slist!=NULL) {
    if (num==GPOINTER_TO_INT(slist->data)) {
      GList *clist;
      gint epos;

      box=(GTK_WIDGET(param->widgets[0])->parent);

      while (box !=NULL&&!GTK_IS_BOX(box)) {
	box=box->parent;
      }

      if (box==NULL) return;

      clist=gtk_container_get_children(GTK_CONTAINER(box));
      epos=g_list_index(clist,param->widgets[0]);
      g_list_free(clist);

      buttond = gtk_file_chooser_button_new(_("LiVES: Select file"),GTK_FILE_CHOOSER_ACTION_OPEN);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(buttond),g_get_current_dir());
      gtk_box_pack_start(GTK_BOX(box),buttond,FALSE,FALSE,10);
      gtk_box_reorder_child(GTK_BOX(box),buttond,epos); // insert after label, before textbox

      if (!GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(param->widgets[0]))) gtk_widget_set_sensitive(buttond,FALSE);

      g_signal_connect (GTK_FILE_CHOOSER(buttond), "selection-changed",G_CALLBACK (on_fileread_clicked),(gpointer)param->widgets[0]);

      if (GTK_IS_ENTRY(param->widgets[0])) gtk_entry_set_max_length(GTK_ENTRY (param->widgets[0]),PATH_MAX);

    }

    slist=slist->next;
  }


  // password fields

  slist=passwd_widgets;
  while (slist!=NULL) {
    if (num==GPOINTER_TO_INT(slist->data)) {
      box=(GTK_WIDGET(param->widgets[0])->parent);

      while (!GTK_IS_VBOX(box)) {
	box=box->parent;
	if (box==NULL) continue;
      }

      hbox = gtk_hbox_new (FALSE, 10);
      gtk_box_pack_start (GTK_BOX (GTK_WIDGET (box)), hbox, FALSE, FALSE, 10);
      
      eventbox=gtk_event_box_new();
      gtk_box_pack_start (GTK_BOX (GTK_WIDGET (hbox)), eventbox, FALSE, FALSE, 10);
      label=gtk_label_new (_("Display Password"));

      checkbutton = gtk_check_button_new ();
      gtk_box_pack_start (GTK_BOX (GTK_WIDGET (hbox)), checkbutton, FALSE, FALSE, 10);
      gtk_button_set_focus_on_click (GTK_BUTTON(checkbutton),FALSE);

      gtk_container_add(GTK_CONTAINER(eventbox),label);
      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (label_act_toggle),
			checkbutton);

      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }

      if (!GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(param->widgets[0]))) gtk_widget_set_sensitive(checkbutton,FALSE);
      gtk_widget_show_all(hbox);

      g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			      G_CALLBACK (passwd_toggle_vis),
			      (gpointer)param->widgets[0]);



      gtk_entry_set_visibility(GTK_ENTRY(param->widgets[0]),FALSE);

    }
    slist=slist->next;
  }
}



void after_aspect_width_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aspect.checkbutton))) {
    gboolean keepeven=FALSE;
    gint width=gtk_spin_button_get_value_as_int (spinbutton);
    gint height=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (aspect.height_widget));
    g_signal_handler_block (aspect.height_widget,aspect.height_func);

    if (((cfile->hsize>>1)<<1)==cfile->hsize&&((cfile->vsize>>1)<<1)==cfile->vsize) {
      // try to keep even
      keepeven=TRUE;
    }
    height=(gint)(width*cfile->vsize/cfile->hsize+.5);

    if (keepeven&&((height>>1)<<1)!=height) {
      gint owidth=width;
      height--;
      width=(gint)(height*cfile->hsize/cfile->vsize+.5);
      if (width!=owidth) {
	height+=2;
	width=owidth;
      }
    }

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (aspect.height_widget), (gdouble)height);
    g_signal_handler_unblock (aspect.height_widget,aspect.height_func);
  }
}


void after_aspect_height_changed (GtkToggleButton *spinbutton, gpointer user_data){
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (aspect.checkbutton))) {
    gboolean keepeven=FALSE;
    gint height=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinbutton));
    gint width=gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (aspect.width_widget));

    g_signal_handler_block (aspect.width_widget,aspect.width_func);

    if (((cfile->hsize>>1)<<1)==cfile->hsize&&((cfile->vsize>>1)<<1)==cfile->vsize) {
      // try to keep even
      keepeven=TRUE;
    }

    width=(gint)(height*cfile->hsize/cfile->vsize+.5);

    if (keepeven&&((width>>1)<<1)!=width) {
      gint oheight=height;
      width--;
      height=(gint)(width*cfile->vsize/cfile->hsize+.5);
      if (height!=oheight) {
	width+=2;
	height=oheight;
      }
    }

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (aspect.width_widget), (gdouble)width);
    g_signal_handler_unblock (aspect.width_widget,aspect.width_func);
  }
}


void special_cleanup (void) {
  // free some memory now

  mainw->framedraw=mainw->framedraw_reset=NULL;
  mainw->framedraw_spinbutton=NULL;


  if (mainw->fd_layer!=NULL) weed_layer_free(mainw->fd_layer);
  mainw->fd_layer=NULL;

  if (mainw->fd_layer_orig!=NULL) weed_layer_free(mainw->fd_layer_orig);
  mainw->fd_layer_orig=NULL;

  mainw->framedraw_preview=NULL;

  if (framedraw.extra_params!=NULL) g_free(framedraw.extra_params);

  if (fileread!=NULL) g_list_free(fileread);
  if (passwd_widgets!=NULL) g_list_free(passwd_widgets);

  framedraw.added=FALSE;
}



void setmergealign (void) {
  lives_param_t *param;
  gint cb_frames=clipboard->frames;

  // TODO - tidy

  if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
    cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
  }

  if (cfile->end-cfile->start+1>(cb_frames*gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON (merge_opts->spinbutton_loops)))&&!merge_opts->loop_to_fit) {
    // set special transalign widgets to their default values
    if (mergealign.start_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.start_widget)&&(param=&(mergealign.rfx->params[mergealign.start_param]))->type==LIVES_PARAM_NUM) {
      if (param->dp) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.start_widget),get_double_param (param->def));
      }
      else {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.start_widget),(gdouble)get_int_param (param->def));
      }
    }
    if (mergealign.end_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.end_widget)&&(param=&(mergealign.rfx->params[mergealign.end_param]))->type==LIVES_PARAM_NUM) {
      if (param->dp) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.end_widget),get_double_param (param->def));
      }
      else {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.end_widget),(gdouble)get_int_param (param->def));
      }
    }
  }
  else {
    if (merge_opts->align_start) {
      // set special transalign widgets to min/max values
      if (mergealign.start_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.start_widget)&&(param=&(mergealign.rfx->params[mergealign.start_param]))->type==LIVES_PARAM_NUM) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.start_widget),(gdouble)param->min);
      }
      if (mergealign.end_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.end_widget)&&(param=&(mergealign.rfx->params[mergealign.end_param]))->type==LIVES_PARAM_NUM) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.end_widget),(gdouble)param->max);
      }
    }
    else {
      // set special transalign widgets to max/min values
      if (mergealign.start_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.start_widget)&&(param=&(mergealign.rfx->params[mergealign.start_param]))->type==LIVES_PARAM_NUM) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.start_widget),(gdouble)param->max);
      }
      if (mergealign.end_widget!=NULL&&GTK_IS_SPIN_BUTTON (mergealign.end_widget)&&(param=&(mergealign.rfx->params[mergealign.end_param]))->type==LIVES_PARAM_NUM) {
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (mergealign.end_widget),(gdouble)param->min);
      }
    }
  }
}


gboolean mt_framedraw(lives_mt *mt, GdkPixbuf *pixbuf) {
  if (framedraw.added) {
    switch (framedraw.type) {
    case FD_RECT_MULTRECT:
      if (mt->track_index==-1) {
	// TODO - hide widgets
      }
      else {
	//
      }
      break;
    }

    framedraw_redraw(&framedraw,TRUE,pixbuf);
    return TRUE;
  }
  return FALSE;
}


gboolean is_perchannel_multi(lives_rfx_t *rfx, gint i) {
  // updated for weed spec 1.1
  if (rfx->params[i].multi==PVAL_MULTI_PER_CHANNEL) return TRUE;
  return FALSE;
}


