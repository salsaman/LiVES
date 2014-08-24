// paramspecial.c
// LiVES
// (c) G. Finch 2004 - 2013 <salsaman@gmail.com>
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
#include "framedraw.h"

static lives_special_aspect_t aspect;
static lives_special_framedraw_rect_t framedraw;
static GList *fileread;
static GList *passwd_widgets;

// TODO - rewrite all of this more sensibly


void init_special (void) {
  framedraw.type=LIVES_PARAM_SPECIAL_TYPE_NONE;
  aspect.width_param=aspect.height_param=NULL;
  aspect.checkbutton=NULL;
  framedraw.xstart_param=framedraw.ystart_param=framedraw.xend_param=framedraw.yend_param=NULL;
  framedraw.stdwidgets=0;
  framedraw.extra_params=NULL;
  framedraw.num_extra=0;
  framedraw.added=FALSE;
  mergealign.start_param=mergealign.end_param=NULL;
  passwd_widgets=NULL;
  fileread=NULL;
}





void add_to_special (const gchar *sp_string, lives_rfx_t *rfx) {
  gchar **array=g_strsplit (sp_string,"|",-1);
  int num_widgets=get_token_count(sp_string,'|')-2;
  int pnum;

  register int i;

  // TODO - make sure only one of each of these


  if (!strcmp (array[0],"aspect")) {
    aspect.width_param=&rfx->params[atoi(array[1])];
    aspect.height_param=&rfx->params[atoi(array[2])];
  }
  else if (!strcmp (array[0],"mergealign")) {
    // align start/end
    /*    if (fx_dialog[1]!=NULL) {
      g_strfreev(array);
      return;
      }*/
    mergealign.start_param=&rfx->params[atoi(array[1])];
    mergealign.end_param=&rfx->params[atoi(array[2])];
    mergealign.rfx=rfx;
  }
  else if (!strcmp (array[0],"framedraw")) {
    if (fx_dialog[1]!=NULL) {
      g_strfreev(array);
      return;
    }
    framedraw.rfx=rfx;
    if (!strcmp (array[1],"rectdemask")) {
      framedraw.type=LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK;
      framedraw.xstart_param=&rfx->params[atoi (array[2])];
      framedraw.ystart_param=&rfx->params[atoi (array[3])];
      framedraw.xend_param=&rfx->params[atoi (array[4])];
      framedraw.yend_param=&rfx->params[atoi (array[5])];
      framedraw.stdwidgets=4;
    }
    else if (!strcmp (array[1],"multrect")) {
      framedraw.type=LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT;
      framedraw.xstart_param=&rfx->params[atoi (array[2])];
      framedraw.ystart_param=&rfx->params[atoi (array[3])];
      framedraw.xend_param=&rfx->params[atoi (array[4])];
      framedraw.yend_param=&rfx->params[atoi (array[5])];
      framedraw.stdwidgets=4;
    }
    else if (!strcmp (array[1],"multirect")) {
      framedraw.type=LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT;
      framedraw.xstart_param=&rfx->params[atoi (array[2])];
      framedraw.ystart_param=&rfx->params[atoi (array[3])];
      framedraw.xend_param=&rfx->params[atoi (array[4])];
      framedraw.yend_param=&rfx->params[atoi (array[5])];
      framedraw.stdwidgets=4;
    }
    else if (!strcmp (array[1],"singlepoint")) {
      framedraw.type=LIVES_PARAM_SPECIAL_TYPE_SINGLEPOINT;
      framedraw.xstart_param=&rfx->params[atoi (array[2])];
      framedraw.ystart_param=&rfx->params[atoi (array[3])];
      framedraw.stdwidgets=2;
    }

    if (num_widgets>framedraw.stdwidgets) framedraw.extra_params=
				  (gint *)g_malloc(((framedraw.num_extra=(num_widgets-framedraw.stdwidgets)))*sizint);

    for (i=0;i<num_widgets;i++) {
      pnum=atoi(array[i+2]);
      if (rfx->status==RFX_STATUS_WEED) {
	if (mainw->multitrack!=NULL) {
	  if (rfx->params[pnum].multi==PVAL_MULTI_PER_CHANNEL) {
	    if ((rfx->params[pnum].hidden&HIDDEN_MULTI)==HIDDEN_MULTI) {
	      if (mainw->multitrack->track_index!=-1) {
		rfx->params[pnum].hidden^=HIDDEN_MULTI; // multivalues allowed
	      }
	      else {
		rfx->params[pnum].hidden|=HIDDEN_MULTI; // multivalues hidden
	      }
	    }
	  }
	}
      }
      if (i>=framedraw.stdwidgets) framedraw.extra_params[i-framedraw.stdwidgets]=pnum;
    }

    if (mainw->multitrack!=NULL) {
      mainw->multitrack->framedraw=&framedraw;
      lives_widget_set_bg_color (mainw->multitrack->fd_frame, LIVES_WIDGET_STATE_NORMAL, &palette->light_red);
    }
  }

  // can be multiple of each of these

  else if (!strcmp (array[0],"fileread")) {
    int idx=atoi(array[1]);
    fileread=g_list_append(fileread,(gpointer)&rfx->params[idx]);

    // ensure we get an entry and not a text_view
    if ((gint)rfx->params[idx].max>RFX_TEXT_MAGIC) rfx->params[idx].max=(gdouble)RFX_TEXT_MAGIC;
  }
  else if (!strcmp (array[0],"password")) {
    int idx=atoi(array[1]);
    passwd_widgets=g_list_append(passwd_widgets,(gpointer)&rfx->params[idx]);

    // ensure we get an entry and not a text_view
    if ((gint)rfx->params[idx].max>RFX_TEXT_MAGIC) rfx->params[idx].max=(gdouble)RFX_TEXT_MAGIC;
  }

  g_strfreev (array);
}


void fd_tweak(lives_rfx_t *rfx) {
  if (rfx->props&RFX_PROPS_MAY_RESIZE) {
    if (framedraw.type!=LIVES_PARAM_SPECIAL_TYPE_NONE) {
      // for effects which can resize, and have a special framedraw, we will use original sized image
      lives_widget_hide(mainw->framedraw_preview);
      lives_widget_set_sensitive(mainw->framedraw_spinbutton,TRUE);
      lives_widget_set_sensitive(mainw->framedraw_scale,TRUE);
    }
  }
}


void fd_connect_spinbutton(lives_rfx_t *rfx) {
  framedraw_connect_spinbutton(&framedraw,rfx);
}


static void passwd_toggle_vis(GtkToggleButton *b, gpointer entry) {
  lives_entry_set_visibility(LIVES_ENTRY(entry),lives_toggle_button_get_active(b));
}


void check_for_special (lives_rfx_t *rfx, lives_param_t *param, LiVESBox *pbox) {
  LiVESWidget *checkbutton;
  LiVESWidget *hbox;
  LiVESWidget *box;
  LiVESWidget *buttond;
  GList *slist;


  gchar *tmp,*tmp2;

  // check if this parameter is part of a special window
  // as we are drawing the paramwindow

  if (param==framedraw.xstart_param) {
    param->special_type=framedraw.type;
    param->special_type_index=0;
    if (framedraw.type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),0.);
    g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
  }
  if (param==framedraw.ystart_param) {
    param->special_type=framedraw.type;
    param->special_type_index=1;
    if (framedraw.type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),0.);
    g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
  }
  if (mainw->current_file>-1) {
    if (param==framedraw.xend_param) {
      param->special_type=framedraw.type;
      param->special_type_index=2;
      if (framedraw.type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->hsize);
      g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
    }
    if (param==framedraw.yend_param) {
      param->special_type=framedraw.type;
      param->special_type_index=3;
      if (framedraw.type==LIVES_PARAM_SPECIAL_TYPE_RECT_DEMASK)
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->vsize);
      g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed", G_CALLBACK (after_framedraw_widget_changed), &framedraw);
    }


    if (framedraw.stdwidgets>0&&!framedraw.added) {
      if (framedraw.xstart_param!=NULL&&framedraw.xstart_param->widgets[0]!=NULL&&
	  framedraw.ystart_param!=NULL&&framedraw.ystart_param->widgets[0]!=NULL) {
	if (framedraw.stdwidgets==2||(framedraw.xend_param!=NULL&&framedraw.xend_param->widgets[0]!=NULL&&
			    framedraw.yend_param!=NULL&&framedraw.yend_param->widgets[0]!=NULL)) {
	  if (mainw->multitrack==NULL) {
	    framedraw_connect(&framedraw,cfile->hsize,cfile->vsize,rfx); // turn passive preview->active
	    framedraw_add_reset(LIVES_VBOX(LIVES_WIDGET(pbox)),&framedraw);
	  }
	  else {
	    mainw->framedraw=mainw->play_image;
	  }
	  framedraw.added=TRUE;
	}
      }
    }
    
    if (param==aspect.width_param) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->hsize);
      aspect.width_func=g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed",
						G_CALLBACK (after_aspect_width_changed),
						NULL);
    }
    if (param==aspect.height_param) {
      lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)cfile->vsize);
      aspect.height_func=g_signal_connect_after (GTK_OBJECT (param->widgets[0]), "value_changed",
						 G_CALLBACK (after_aspect_height_changed),
						 NULL);
      
      box = lives_hbox_new (FALSE, 0);
      lives_box_pack_start (LIVES_BOX (LIVES_WIDGET (pbox)), box, FALSE, FALSE, widget_opts.packing_height*2);
      
      
      add_fill_to_box(LIVES_BOX(box));
      
      checkbutton = lives_standard_check_button_new ((tmp=g_strdup(_("Maintain _Aspect Ratio"))),TRUE,
						     LIVES_BOX(box),(tmp2=g_strdup(_("Maintain aspect ratio of original frame"))));

      g_free(tmp); g_free(tmp2);

      add_fill_to_box(LIVES_BOX(box));

      lives_widget_show_all(box);

      aspect.checkbutton=checkbutton;
    }
  }

  slist=fileread;
  while (slist!=NULL) {
    if (param==(lives_param_t *)(slist->data)) {
      GList *clist;
      gint epos;

      param->special_type=LIVES_PARAM_SPECIAL_TYPE_FILEREAD;

      if (param->widgets[0]==NULL) continue;

      box=lives_widget_get_parent(param->widgets[0]);

      while (box!=NULL&&!LIVES_IS_HBOX(box)) {
	box=lives_widget_get_parent(box);
      }

      if (box==NULL) return;

      clist=gtk_container_get_children(LIVES_CONTAINER(box));
      epos=g_list_index(clist,param->widgets[0]);
      g_list_free(clist);

      buttond = lives_standard_file_button_new (FALSE,g_get_current_dir());
      lives_box_pack_start(LIVES_BOX(box),buttond,FALSE,FALSE,widget_opts.packing_width);
      lives_box_reorder_child(LIVES_BOX(box),buttond,epos); // insert after label, before textbox
      g_signal_connect(buttond, "clicked", G_CALLBACK (on_filesel_button_clicked), (gpointer)param->widgets[0]);

      if (!lives_widget_is_sensitive(param->widgets[0])) lives_widget_set_sensitive(buttond,FALSE);

      if (GTK_IS_ENTRY(param->widgets[0])) {
	lives_entry_set_editable(LIVES_ENTRY(param->widgets[0]),FALSE);
	if (param->widgets[1]!=NULL&&
	    LIVES_IS_LABEL(param->widgets[1])&&
	    gtk_label_get_mnemonic_widget(LIVES_LABEL(param->widgets[1]))!=NULL) 
	  gtk_label_set_mnemonic_widget (LIVES_LABEL(param->widgets[1]),buttond);
	lives_entry_set_max_length(LIVES_ENTRY (param->widgets[0]),PATH_MAX);
      }
    }

    slist=slist->next;
  }


  // password fields

  slist=passwd_widgets;
  while (slist!=NULL) {
    if (param==(lives_param_t *)(slist->data)) {
      if (param->widgets[0]==NULL) continue;

      box=lives_widget_get_parent(param->widgets[0]);

      param->special_type=LIVES_PARAM_SPECIAL_TYPE_PASSWORD;

      while (!LIVES_IS_VBOX(box)) {
	box=lives_widget_get_parent(box);
	if (box==NULL) continue;
      }

      hbox = lives_hbox_new (FALSE, 0);
      lives_box_pack_start (LIVES_BOX (LIVES_WIDGET (box)), hbox, FALSE, FALSE, widget_opts.packing_height);
      
      checkbutton = lives_standard_check_button_new (_("Display Password"),FALSE,LIVES_BOX(hbox),NULL);

      gtk_button_set_focus_on_click (LIVES_BUTTON(checkbutton),FALSE);

      if (!lives_widget_is_sensitive(param->widgets[0])) lives_widget_set_sensitive(checkbutton,FALSE);
      lives_widget_show_all(hbox);

      g_signal_connect_after (GTK_OBJECT (checkbutton), "toggled",
			      G_CALLBACK (passwd_toggle_vis),
			      (gpointer)param->widgets[0]);



      lives_entry_set_visibility(LIVES_ENTRY(param->widgets[0]),FALSE);

    }
    slist=slist->next;
  }
}



void after_aspect_width_changed (GtkSpinButton *spinbutton, gpointer user_data) {
  if (lives_toggle_button_get_active (LIVES_TOGGLE_BUTTON (aspect.checkbutton))) {
    boolean keepeven=FALSE;
    gint width=lives_spin_button_get_value_as_int (spinbutton);
    gint height=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (aspect.height_param->widgets[0]));
    g_signal_handler_block (aspect.height_param->widgets[0],aspect.height_func);

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

    lives_spin_button_set_value (LIVES_SPIN_BUTTON (aspect.height_param->widgets[0]), (gdouble)height);
    g_signal_handler_unblock (aspect.height_param->widgets[0],aspect.height_func);
  }
}


void after_aspect_height_changed (GtkToggleButton *spinbutton, gpointer user_data){
  if (lives_toggle_button_get_active (LIVES_TOGGLE_BUTTON (aspect.checkbutton))) {
    boolean keepeven=FALSE;
    gint height=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (spinbutton));
    gint width=lives_spin_button_get_value_as_int (LIVES_SPIN_BUTTON (aspect.width_param->widgets[0]));

    g_signal_handler_block (aspect.width_param->widgets[0],aspect.width_func);

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

    lives_spin_button_set_value (LIVES_SPIN_BUTTON (aspect.width_param->widgets[0]), (gdouble)width);
    g_signal_handler_unblock (aspect.width_param->widgets[0],aspect.width_func);
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


void set_aspect_ratio_widgets (lives_param_t *w, lives_param_t *h) {
  aspect.width_param=w;
  aspect.height_param=h;
}


void setmergealign (void) {
  lives_param_t *param;
  gint cb_frames=clipboard->frames;

  if (prefs->ins_resample&&clipboard->fps!=cfile->fps) {
    cb_frames=count_resampled_frames(clipboard->frames,clipboard->fps,cfile->fps);
  }

  if (cfile->end-cfile->start+1>(cb_frames*((merge_opts!=NULL&&merge_opts->spinbutton_loops!=NULL)?
					    lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON (merge_opts->spinbutton_loops)):1))
      &&!merge_opts->loop_to_fit) {
    // set special transalign widgets to their default values
    if (mergealign.start_param!=NULL&&mergealign.start_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON
	(mergealign.start_param->widgets[0])&&(param=mergealign.start_param)->type==LIVES_PARAM_NUM) {
      if (param->dp) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),get_double_param (param->def));
      }
      else {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)get_int_param (param->def));
      }
    }
    if (mergealign.end_param!=NULL&&mergealign.end_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON 
	(mergealign.end_param->widgets[0])&&(param=mergealign.end_param)->type==LIVES_PARAM_NUM) {
      if (param->dp) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),get_double_param (param->def));
      }
      else {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)get_int_param (param->def));
      }
    }
  }
  else {
    if (merge_opts->align_start) {
      // set special transalign widgets to min/max values
      if (mergealign.start_param!=NULL&&mergealign.start_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON 
	  (mergealign.start_param->widgets[0])&&(param=mergealign.start_param)->type==LIVES_PARAM_NUM) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)param->min);
      }
      if (mergealign.end_param!=NULL&&mergealign.end_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON 
	  (mergealign.end_param->widgets[0])&&(param=mergealign.end_param)->type==LIVES_PARAM_NUM) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)param->max);
      }
    }
    else {
      // set special transalign widgets to max/min values
      if (mergealign.start_param!=NULL&&mergealign.start_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON 
	  (mergealign.start_param->widgets[0])&&(param=mergealign.start_param)->type==LIVES_PARAM_NUM) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)param->max);
      }
      if (mergealign.end_param!=NULL&&mergealign.end_param->widgets[0]!=NULL&&GTK_IS_SPIN_BUTTON 
	  (mergealign.end_param->widgets[0])&&(param=mergealign.end_param)->type==LIVES_PARAM_NUM) {
	lives_spin_button_set_value (LIVES_SPIN_BUTTON (param->widgets[0]),(gdouble)param->min);
      }
    }
  }
}


LiVESPixbuf *mt_framedraw(lives_mt *mt, LiVESPixbuf *pixbuf) {
  if (framedraw.added) {
    switch (framedraw.type) {
    case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTRECT:
    case LIVES_PARAM_SPECIAL_TYPE_RECT_MULTIRECT:
      if (mt->track_index==-1) {
	// TODO - hide widgets
      }
      else {
	//
      }
      break;

    default:
      break;

    }

    framedraw_redraw(&framedraw,TRUE,pixbuf);
    pixbuf=layer_to_pixbuf(mainw->fd_layer);

    weed_plant_free(mainw->fd_layer);
    mainw->fd_layer=NULL;

    return pixbuf;
  }
  return pixbuf;
}


boolean is_perchannel_multi(lives_rfx_t *rfx, int i) {
  // updated for weed spec 1.1
  if (rfx->params[i].multi==PVAL_MULTI_PER_CHANNEL) return TRUE;
  return FALSE;
}


