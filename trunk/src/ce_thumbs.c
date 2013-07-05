// ce_thumbs.c
// LiVES
// (c) G. Finch 2013 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// clip thumbnails window for dual head mode


#include "main.h"
#include "effects-weed.h"

static LiVESWidget **combo_entries;

static boolean switch_clip_cb (LiVESWidget *eventbox, LiVESXEventButton *event, gpointer user_data) {
  int i=GPOINTER_TO_INT(user_data);
  if (mainw->playing_file==-1) return FALSE;
  switch_clip (0,i);
  return FALSE;
}




#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget
static LiVESWidget *hbox;
#endif

void start_ce_thumb_mode(void) {
#if GTK_CHECK_VERSION(3,2,0)  // required for grid widget

  LiVESWidget *thumb_image=NULL;
  LiVESWidget *vbox;
  LiVESWidget *eventbox;
  LiVESWidget *usibl=NULL,*sibl=NULL;
  LiVESWidget *hbox2;
  LiVESWidget *combo;
  LiVESWidget *tscroll;

  LiVESWidget *tgrid=lives_grid_new();

  LiVESPixbuf *thumbnail;

  GList *cliplist=mainw->cliplist;

  GList *fxlist=NULL;

  gchar filename[PATH_MAX];
  gchar *tmp;

  int width=CLIP_THUMB_WIDTH,height=CLIP_THUMB_HEIGHT;
  int modes=rte_getmodespk();
  int cpw,scr_width;

  int count=0;

  register int i,j;

  lives_grid_set_row_spacing (LIVES_GRID(tgrid),width>>1);
  lives_grid_set_column_spacing (LIVES_GRID(tgrid),height>>1);

  // dual monitor mode, the gui monitor can show clip thumbnails
  lives_widget_hide(mainw->eventbox);

  hbox=lives_hbox_new (FALSE, widget_opts.packing_width);
  lives_box_pack_start (LIVES_BOX (mainw->vbox1), hbox, TRUE, TRUE, 0);


  // fx area
  vbox=lives_vbox_new (FALSE, widget_opts.packing_height);
  tscroll=lives_standard_scrolled_window_new(width,height,vbox);
  lives_box_pack_start (LIVES_BOX (hbox), tscroll, TRUE, TRUE, 0);

  combo_entries=(LiVESWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(LiVESWidget *));
  
  for (i=0;i<prefs->rte_keys_virtual;i++) {
    //lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON(key_checks[i]),FALSE);

    fxlist=NULL;

    for (j=0;j<=rte_key_getmaxmode(i+1);j++) {
      fxlist=g_list_append(fxlist,rte_keymode_get_filter_name(i+1,j));
    }

    if (fxlist==NULL) continue;

    hbox2 = lives_hbox_new (FALSE, 0);
    lives_box_pack_start (LIVES_BOX (vbox), hbox2, FALSE, FALSE, widget_opts.packing_height);

    combo=lives_standard_combo_new(NULL,FALSE,fxlist,LIVES_BOX(hbox2),NULL);

    g_list_free_strings(fxlist);
    g_list_free(fxlist);

    combo_entries[i] = lives_combo_get_entry(LIVES_COMBO(combo));

    lives_entry_set_text (LIVES_ENTRY (combo_entries[i]),(tmp=rte_keymode_get_filter_name(i+1,rte_key_getmode(i+1))));
    g_free(tmp);
 
    lives_entry_set_editable (LIVES_ENTRY (combo_entries[i]), FALSE);
      

    /*    g_signal_connect(GTK_OBJECT (combo), "changed",
	  G_CALLBACK (fx_changed),GINT_TO_POINTER(i*rte_getmodespk()+j));*/

  }


  // insert a scrolled window
  tscroll=lives_standard_scrolled_window_new(width,height,tgrid);
  lives_scrolled_window_set_policy (LIVES_SCROLLED_WINDOW (tscroll), LIVES_POLICY_NEVER, LIVES_POLICY_AUTOMATIC);

  lives_box_pack_start (LIVES_BOX (hbox), tscroll, TRUE, TRUE, 0);

  // add thumbs to grid

  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
  }

  cpw=scr_width*2/width/3;

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

  lives_widget_show_all(hbox);

  mainw->ce_thumbs=TRUE;

#endif
}


void end_ce_thumb_mode(void) {
  lives_widget_destroy(hbox);
  lives_widget_show(mainw->eventbox);
  g_free(combo_entries);
  mainw->ce_thumbs=FALSE;
}
