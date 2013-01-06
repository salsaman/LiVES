// gui.c
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// code for drawing the main window

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "effects.h"
#include "support.h"
#include "rfx-builder.h"
#include "paramwindow.h"
#include "resample.h"
#include "rte_window.h"
#include "stream.h"
#include "startup.h"


#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif


// closures for keys for fade/unfade background
static GClosure *stop_closure;
static GClosure *fullscreen_closure;
static GClosure *dblsize_closure;
static GClosure *sepwin_closure;
static GClosure *loop_closure;
static GClosure *loop_cont_closure;
static GClosure *fade_closure;
static GClosure *showfct_closure;
static GClosure *showsubs_closure;
static GClosure *rec_closure;
static GClosure *mute_audio_closure;
static GClosure *ping_pong_closure;


void 
load_theme (void) {
  // load the theme images
  // TODO - set palette in here ?
  GError *error=NULL;
  gchar *tmp=g_build_filename(prefs->prefix_dir,THEME_DIR,prefs->theme,"main.jpg",NULL);
  mainw->imsep=gdk_pixbuf_new_from_file(tmp,&error);
  g_free(tmp);
  
  if (!(error==NULL)) {
    palette->style=STYLE_PLAIN;
    g_snprintf(prefs->theme,64,"%%ERROR%%");
    g_error_free(error);
  }
  else {
    mainw->sep_image = gtk_image_new_from_pixbuf (mainw->imsep);
    tmp=g_build_filename(prefs->prefix_dir,THEME_DIR,prefs->theme,"frame.jpg",NULL);
    mainw->imframe=gdk_pixbuf_new_from_file(tmp,&error);
    g_free(tmp);
    if (!(error==NULL)) {
      g_error_free(error);
    }
  }
}


void add_message_scroller(GtkWidget *conter) {
  GtkTextBuffer *tbuff=NULL;
  gchar *all_text=NULL;
  GtkTextIter start_iter;
  GtkTextIter end_iter;

  if (mainw->textview1!=NULL) {
    tbuff=gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1));
    gtk_text_buffer_get_start_iter(tbuff,&start_iter);
    gtk_text_buffer_get_end_iter(tbuff,&end_iter);
    all_text=gtk_text_buffer_get_text(tbuff,&start_iter,&end_iter,TRUE);
    gtk_widget_destroy(mainw->textview1);
  }

  if (mainw->scrolledwindow!=NULL) {
    gtk_widget_destroy(mainw->scrolledwindow);
  }

  mainw->scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(mainw->scrolledwindow),GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
  gtk_widget_show (mainw->scrolledwindow);

  gtk_container_add (GTK_CONTAINER(conter), mainw->scrolledwindow);

  mainw->textview1 = gtk_text_view_new ();
  gtk_widget_show (mainw->textview1);
  gtk_container_add (GTK_CONTAINER (mainw->scrolledwindow), mainw->textview1);

  tbuff=gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1));
  if (tbuff!=NULL && all_text!=NULL) {
    GtkTextIter end_iter;
    GtkTextMark *mark;
    gtk_text_buffer_set_text(tbuff,all_text,-1);
    g_free(all_text);
    gtk_text_buffer_get_end_iter(tbuff,&end_iter);
    mark=gtk_text_buffer_create_mark(tbuff,NULL,&end_iter,FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW (mainw->textview1),mark);
    gtk_text_buffer_delete_mark (tbuff,mark);
  }

  gtk_widget_set_size_request (mainw->textview1, -1, 50);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (mainw->textview1), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (mainw->textview1), GTK_WRAP_WORD);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (mainw->textview1), FALSE);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_base(mainw->textview1, GTK_STATE_NORMAL, &palette->info_base);
    gtk_widget_modify_text(mainw->textview1, GTK_STATE_NORMAL, &palette->info_text);
  }
}


void make_custom_submenus(void) {
  mainw->custom_gens_submenu = gtk_menu_item_new_with_mnemonic (_ ("_Custom Generators"));
  mainw->custom_effects_submenu = gtk_menu_item_new_with_mnemonic (_ ("_Custom Effects"));
  mainw->custom_utilities_submenu = gtk_menu_item_new_with_mnemonic (_("Custom _Utilities"));

}


void 
create_LiVES (void)
{
  GtkWidget *hbox1;
  GtkWidget *vbox2;
  GtkWidget *menuitem11;
  GtkWidget *menuitem11_menu;
  GtkWidget *separatormenuitem2;
  GtkWidget *separatormenuitem3;
  GtkWidget *separatormenuitem11;
  GtkWidget *separatormenuitem110;
  GtkWidget *separatormenuitem99;
  GtkWidget *menuitem12;
  GtkWidget *menuitem12_menu;
  GtkWidget *select_submenu_menu;
  GtkWidget *submenu_menu;
  GtkWidget *export_submenu_menu;
  GtkWidget *trimaudio_submenu_menu;
  GtkWidget *delaudio_submenu_menu;
  GtkWidget *menuitemsep;
  GtkWidget *image333;
  GtkWidget *image380;
  GtkWidget *separator3;
  GtkWidget *image334;
  GtkWidget *image1334;
  GtkWidget *separator5;
  GtkWidget *separator27;
  GtkWidget *separator55;
  GtkWidget *separator88;
  GtkWidget *menuitem13;
  GtkWidget *menuitem13_menu;
  GtkWidget *image335;
  GtkWidget *image336;
  GtkWidget *image337;
  GtkWidget *image1337;
  GtkWidget *image355;
  GtkWidget *separator8;
  GtkWidget *effects;
  GtkWidget *separator9;
  GtkWidget *separator19;
  GtkWidget *separator49;
  GtkWidget *separator10;
  GtkWidget *separator6;
  GtkWidget *separator26;
  GtkWidget *image346;
  GtkWidget *tools;
  GtkWidget *audio;
  GtkWidget *audio_menu;
  GtkWidget *info;
  GtkWidget *info_menu;
  GtkWidget *advanced;
  GtkWidget *advanced_menu;
  GtkWidget *rfx_menu;
  GtkWidget *rfx_submenu;
  GtkWidget *midi_menu;
  GtkWidget *midi_submenu;
  GtkWidget *midi_learn;
  GtkWidget *midi_load;
  GtkWidget *midi_save;
  GtkWidget *vj_menu;
  GtkWidget *toys_menu;
  GtkWidget *win;
  GtkWidget *image347;
  GtkWidget *menuitem14;
  GtkWidget *menuitem14_menu;
  GObject *spinbutton_start_adj;
  GObject *spinbutton_end_adj;
  GtkWidget *about;
  GtkWidget *show_manual;
  GtkWidget *email_author;
  GtkWidget *donate;
  GtkWidget *report_bug;
  GtkWidget *suggest_feature;
  GtkWidget *help_translate;
  GtkWidget *vbox4;
  GtkWidget *pf_label;
  GtkWidget *label15;
  GtkWidget *label16;
  GtkWidget *hbox3;
  GtkWidget *t_label;
  GtkWidget *label;

#if defined (HAVE_YUV4MPEG) || defined (HAVE_UNICAP)
  GtkWidget *submenu;
  GtkWidget *menuitem;
#endif 

  GObject *spinbutton_pb_fps_adj;

  GtkWidget *new_test_rfx;
  GtkWidget *copy_rfx;
  GtkWidget *import_custom_rfx;
  GtkWidget *rebuild_rfx;
  GtkWidget *assign_rte_keys;

  GtkWidget *tmp_toolbar_icon;
  GdkColor *normal;
  gchar buff[32768];

  GdkPixbuf *pixbuf;

  gchar *tmp;
  gchar *fnamex;

  stop_closure=NULL;
  fullscreen_closure=NULL;
  dblsize_closure=NULL;
  sepwin_closure=NULL;
  loop_closure=NULL;
  loop_cont_closure=NULL;
  fade_closure=NULL;
  showfct_closure=NULL;
  showsubs_closure=NULL;
  rec_closure=NULL;
  mute_audio_closure=NULL;
  ping_pong_closure=NULL;

  ////////////////////////////////////

  mainw->double_size=FALSE;
  mainw->sep_win=FALSE;

  mainw->current_file=-1;

  mainw->preview_image=NULL;

  mainw->sep_image = gtk_image_new_from_pixbuf (NULL);
  mainw->image272 = gtk_image_new_from_pixbuf (NULL);
  mainw->image273 = gtk_image_new_from_pixbuf (NULL);
  mainw->imframe=mainw->imsep=NULL;

  if (palette->style&STYLE_1) {
    load_theme();
  }

  if (mainw->imframe!=NULL) {
    gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image272),mainw->imframe);
    gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image273),mainw->imframe);
  }

  mainw->accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  
  mainw->layout_textbuffer=gtk_text_buffer_new(NULL);
  g_object_ref(mainw->layout_textbuffer);
  mainw->affected_layouts_map=NULL;

  mainw->LiVES = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  //gtk_window_present(GTK_WINDOW(mainw->LiVES));

  // TODO - can we use just DEFAULT_DROP ?
  gtk_drag_dest_set(mainw->LiVES,GTK_DEST_DEFAULT_ALL,mainw->target_table,2,
		    (GdkDragAction)(GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK));

  g_signal_connect (GTK_OBJECT (mainw->LiVES), "drag-data-received",
		    G_CALLBACK (drag_from_outside),
		    NULL);


  if (capable->smog_version_correct) gtk_window_set_decorated(GTK_WINDOW(mainw->LiVES),prefs->open_decorated);

  if (palette->style==STYLE_PLAIN) {
    // if gtk_widget_ensure_style is used, we can't grab external frames...
    if (!mainw->foreign) {
      gtk_widget_ensure_style(mainw->LiVES);
    }
    normal=&gtk_widget_get_style(mainw->LiVES)->bg[GTK_STATE_NORMAL];
    colour_equal((GdkColor *)(&palette->normal_back),normal);

    normal=&gtk_widget_get_style(mainw->LiVES)->fg[GTK_STATE_NORMAL];
    colour_equal((GdkColor *)(&palette->normal_fore),normal);
  }

 
  gtk_window_set_title (GTK_WINDOW (mainw->LiVES), "LiVES");

  mainw->vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (mainw->LiVES), mainw->vbox1);
  gtk_widget_show (mainw->vbox1);

  mainw->menu_hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (mainw->menu_hbox);
  gtk_box_pack_start (GTK_BOX (mainw->vbox1), mainw->menu_hbox, FALSE, FALSE, 0);

  mainw->menubar = gtk_menu_bar_new ();
  gtk_widget_show (mainw->menubar);
  gtk_box_pack_start (GTK_BOX (mainw->menu_hbox), mainw->menubar, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->menubar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  menuitem11 = gtk_menu_item_new_with_mnemonic (_("_File"));
  gtk_widget_show (menuitem11);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), menuitem11);

  menuitem11_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem11), menuitem11_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem11_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->open = gtk_menu_item_new_with_mnemonic (_("_Open File/Directory"));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open);
  gtk_widget_add_accelerator (mainw->open, "activate", mainw->accel_group,
                              GDK_o, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->open_sel = gtk_menu_item_new_with_mnemonic (_("O_pen Part of File..."));

  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open_sel);



  mainw->open_loc = gtk_menu_item_new_with_mnemonic (_("Open _Location/Stream..."));

#ifdef HAVE_WEBM

  mainw->open_loc_menu = gtk_menu_item_new_with_mnemonic (_("Open _Location/Stream..."));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open_loc_menu);

  mainw->open_loc_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->open_loc_menu), mainw->open_loc_submenu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->open_loc_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->open_utube = gtk_menu_item_new_with_mnemonic (_("Open _Youtube Clip..."));
  gtk_container_add (GTK_CONTAINER (mainw->open_loc_submenu), mainw->open_utube);

  gtk_container_add (GTK_CONTAINER (mainw->open_loc_submenu), mainw->open_loc);

#else

  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open_loc);

#endif

  mainw->open_vcd_menu = gtk_menu_item_new_with_mnemonic (_("Import from _dvd/vcd..."));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open_vcd_menu);
  mainw->open_vcd_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->open_vcd_menu), mainw->open_vcd_submenu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->open_vcd_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->open_dvd = gtk_menu_item_new_with_mnemonic (_("Import from _dvd"));
  gtk_container_add (GTK_CONTAINER (mainw->open_vcd_submenu), mainw->open_dvd);

  mainw->open_vcd = gtk_menu_item_new_with_mnemonic (_("Import from _vcd"));
  gtk_container_add (GTK_CONTAINER (mainw->open_vcd_submenu), mainw->open_vcd);

  mainw->open_device_menu = gtk_menu_item_new_with_mnemonic (_("_Import from Firewire"));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->open_device_menu);
  mainw->open_device_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->open_device_menu), mainw->open_device_submenu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->open_device_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->open_firewire = gtk_menu_item_new_with_mnemonic (_("Import from _Firewire Device (dv)"));
  mainw->open_hfirewire = gtk_menu_item_new_with_mnemonic (_("Import from _Firewire Device (hdv)"));

#ifdef HAVE_LDVGRAB
  gtk_container_add (GTK_CONTAINER (mainw->open_device_submenu), mainw->open_firewire);
  gtk_container_add (GTK_CONTAINER (mainw->open_device_submenu), mainw->open_hfirewire);
  gtk_widget_show (mainw->open_firewire);
  gtk_widget_show (mainw->open_hfirewire);
  gtk_widget_show (mainw->open_device_menu);
  gtk_widget_show (mainw->open_device_submenu);
#endif

  gtk_widget_show (mainw->open);

  if (capable->has_mplayer) {
    gtk_widget_show (mainw->open_sel);
#ifdef ENABLE_DVD_GRAB
    gtk_widget_show (mainw->open_vcd_menu);
    gtk_widget_show (mainw->open_vcd_submenu);
    gtk_widget_show (mainw->open_dvd);
    gtk_widget_show (mainw->open_vcd);
#endif
#ifdef HAVE_WEBM
    gtk_widget_show_all (mainw->open_loc_menu);
#else
    gtk_widget_show (mainw->open_loc);
#endif
  }

  mainw->add_live_menu = gtk_menu_item_new_with_mnemonic (_("_Add Webcam/TV card..."));

#if defined(HAVE_UNICAP) || defined(HAVE_YUV4MPEG)
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->add_live_menu);
  gtk_widget_show (mainw->add_live_menu);

#ifndef HAVE_UNICAP
  if (capable->has_mplayer) {
#endif

    submenu=gtk_menu_new();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->add_live_menu), submenu);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_bg(submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
    }
    gtk_widget_show (submenu);

#ifdef HAVE_UNICAP
  menuitem = gtk_menu_item_new_with_mnemonic (_("Add _Unicap Device"));
  gtk_container_add (GTK_CONTAINER (submenu), menuitem);
  gtk_widget_show (menuitem);
  g_signal_connect (GTK_OBJECT (menuitem), "activate",
		    G_CALLBACK (on_open_vdev_activate),
		    NULL);
#endif

#ifdef HAVE_YUV4MPEG
    if (capable->has_dvgrab) {
      menuitem = gtk_menu_item_new_with_mnemonic (_("Add Live _Firewire Device"));
      gtk_container_add (GTK_CONTAINER (submenu), menuitem);
      gtk_widget_show (menuitem);

      g_signal_connect (GTK_OBJECT (menuitem), "activate",
			G_CALLBACK (on_live_fw_activate),
			NULL);
    }

    menuitem = gtk_menu_item_new_with_mnemonic (_("Add _TV Device"));
    gtk_container_add (GTK_CONTAINER (submenu), menuitem);
    gtk_widget_show (menuitem);

    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (on_live_tvcard_activate),
		      NULL);

#ifndef HAVE_UNICAP
  } // if (capable->has_mplayer)
#endif

#endif
#endif // defined HAVE_UNICAP || defined HAVE_YUV4MPEG

  mainw->recent_menu = gtk_menu_item_new_with_mnemonic (_("_Recent Files..."));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->recent_menu);
  mainw->recent_submenu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->recent_menu), mainw->recent_submenu);

  memset(buff,0,1);

  // since we are still initialising, we need to check if we can read prefs
  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent1",buff,32768);
  }
  mainw->recent1 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mainw->recent1);
  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent2",buff,32768);
  }
  mainw->recent2 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mainw->recent2);

  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent3",buff,32768);
  }
  mainw->recent3 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mainw->recent3);

  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent4",buff,32768);
  }
  mainw->recent4 = gtk_menu_item_new_with_label (buff);
  if (strlen (buff)) gtk_widget_show (mainw->recent4);

  gtk_container_add (GTK_CONTAINER (mainw->recent_submenu), mainw->recent1);
  gtk_container_add (GTK_CONTAINER (mainw->recent_submenu), mainw->recent2);
  gtk_container_add (GTK_CONTAINER (mainw->recent_submenu), mainw->recent3);
  gtk_container_add (GTK_CONTAINER (mainw->recent_submenu), mainw->recent4);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->recent_submenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  
  gtk_widget_show (mainw->recent_submenu);

  if (capable->smog_version_correct&&prefs->show_recent) {
    gtk_widget_show (mainw->recent_menu);
  }

  separatormenuitem11 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem11);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), separatormenuitem11);
  gtk_widget_set_sensitive (separatormenuitem11, FALSE);

  mainw->vj_load_set = gtk_menu_item_new_with_mnemonic (_("_Reload Clip Set..."));
  gtk_widget_show (mainw->vj_load_set);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->vj_load_set);

  mainw->vj_save_set = gtk_menu_item_new_with_mnemonic (_("Close/Sa_ve All Clips"));
  gtk_widget_show (mainw->vj_save_set);
  gtk_widget_set_sensitive (mainw->vj_save_set, FALSE);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->vj_save_set);

  separator88 = gtk_menu_item_new ();
  gtk_widget_show (separator88);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), separator88);
  gtk_widget_set_sensitive (separator88, FALSE);

  mainw->save_as = gtk_image_menu_item_new_from_stock ("gtk-save", mainw->accel_group);
  gtk_container_add (GTK_CONTAINER(menuitem11_menu), mainw->save_as);
  gtk_widget_set_sensitive (mainw->save_as, FALSE);
  set_menu_text(mainw->save_as,_("_Encode Clip As..."),TRUE);

  mainw->save_selection = gtk_menu_item_new_with_mnemonic (_("Encode _Selection As..."));
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->save_selection);
  gtk_widget_set_sensitive (mainw->save_selection, FALSE);

  gtk_widget_show (mainw->save_as);
  gtk_widget_show (mainw->save_selection);

  mainw->close = gtk_menu_item_new_with_mnemonic (_("_Close This Clip"));
  gtk_widget_add_accelerator (mainw->close, "activate", mainw->accel_group,
                              GDK_w, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (mainw->close);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->close);
  gtk_widget_set_sensitive (mainw->close, FALSE);

  separatormenuitem99 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem99);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), separatormenuitem99);
  gtk_widget_set_sensitive (separatormenuitem99, FALSE);

  mainw->backup = gtk_menu_item_new_with_mnemonic (_("_Backup Clip as .lv1..."));
  gtk_widget_show (mainw->backup);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->backup);
  gtk_widget_set_sensitive (mainw->backup, FALSE);

  gtk_widget_add_accelerator (mainw->backup, "activate", mainw->accel_group,
                              GDK_b, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->restore = gtk_menu_item_new_with_mnemonic (_("_Restore Clip from .lv1..."));
  gtk_widget_show (mainw->restore);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->restore);

  gtk_widget_add_accelerator (mainw->restore, "activate", mainw->accel_group,
                              GDK_r, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  separatormenuitem2 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem2);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), separatormenuitem2);
  gtk_widget_set_sensitive (separatormenuitem2, FALSE);

  mainw->sw_sound = gtk_check_menu_item_new_with_mnemonic (_("Encode/Load/Backup _with Sound"));
  gtk_widget_show (mainw->sw_sound);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->sw_sound);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->sw_sound),TRUE);

  mainw->aload_subs = gtk_check_menu_item_new_with_mnemonic (_("Auto load subtitles"));
  gtk_widget_show (mainw->aload_subs);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->aload_subs);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->aload_subs),prefs->autoload_subs);

  separatormenuitem3 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem3);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), separatormenuitem3);
  gtk_widget_set_sensitive (separatormenuitem3, FALSE);

  mainw->clear_ds = gtk_menu_item_new_with_mnemonic (_("Clean _up Diskspace"));
  gtk_widget_show (mainw->clear_ds);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->clear_ds);

  mainw->quit = gtk_image_menu_item_new_from_stock ("gtk-quit", mainw->accel_group);
  gtk_widget_show (mainw->quit);
  gtk_container_add (GTK_CONTAINER (menuitem11_menu), mainw->quit);

  menuitem12 = gtk_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_widget_show (menuitem12);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), menuitem12);

  menuitem12_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem12), menuitem12_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem12_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->undo = gtk_image_menu_item_new_with_mnemonic (_("_Undo"));
  gtk_widget_show (mainw->undo);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->undo);
  gtk_widget_set_sensitive (mainw->undo, FALSE);

  gtk_widget_add_accelerator (mainw->undo, "activate", mainw->accel_group,
                              GDK_u, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image333 = gtk_image_new_from_stock ("gtk-undo", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image333);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->undo), image333);

  mainw->redo = gtk_image_menu_item_new_with_mnemonic (_("_Redo"));
  gtk_widget_hide (mainw->redo);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->redo);
  gtk_widget_set_sensitive (mainw->redo, FALSE);

  gtk_widget_add_accelerator (mainw->redo, "activate", mainw->accel_group,
                              GDK_z, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image380 = gtk_image_new_from_stock ("gtk-redo", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image380);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->redo), image380);

  separator3 = gtk_menu_item_new ();
  gtk_widget_show (separator3);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), separator3);
  gtk_widget_set_sensitive (separator3, FALSE);

  mainw->mt_menu = gtk_image_menu_item_new_with_mnemonic (_("_MULTITRACK mode"));
  gtk_widget_show (mainw->mt_menu);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->mt_menu);

  separator3 = gtk_menu_item_new ();
  gtk_widget_show (separator3);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), separator3);
  gtk_widget_set_sensitive (separator3, FALSE);

  gtk_widget_add_accelerator (mainw->mt_menu, "activate", mainw->accel_group,
                              GDK_m, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->copy = gtk_image_menu_item_new_with_mnemonic (_("_Copy Selection"));
  gtk_widget_show (mainw->copy);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->copy);
  gtk_widget_set_sensitive (mainw->copy, FALSE);

  gtk_widget_add_accelerator (mainw->copy, "activate", mainw->accel_group,
                              GDK_c, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->cut = gtk_image_menu_item_new_with_mnemonic (_("Cu_t Selection"));
  gtk_widget_show (mainw->cut);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->cut);
  gtk_widget_set_sensitive (mainw->cut, FALSE);

  gtk_widget_add_accelerator (mainw->cut, "activate", mainw->accel_group,
                              GDK_t, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->insert = gtk_image_menu_item_new_with_mnemonic (_("_Insert from Clipboard..."));
  gtk_widget_show (mainw->insert);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->insert);
  gtk_widget_set_sensitive (mainw->insert, FALSE);

  gtk_widget_add_accelerator (mainw->insert, "activate", mainw->accel_group,
                              GDK_i, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image334 = gtk_image_new_from_stock ("gtk-add", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image334);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->insert), image334);

  mainw->paste_as_new = gtk_image_menu_item_new_with_mnemonic (_("Paste as _New"));
  gtk_widget_show (mainw->paste_as_new);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->paste_as_new);
  gtk_widget_set_sensitive (mainw->paste_as_new, FALSE);

  gtk_widget_add_accelerator (mainw->paste_as_new, "activate", mainw->accel_group,
                              GDK_n, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->merge = gtk_menu_item_new_with_mnemonic (_("_Merge Clipboard with Selection..."));
  if (capable->has_composite&&capable->has_convert) {
    gtk_widget_show (mainw->merge);
  }
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->merge);
  gtk_widget_set_sensitive (mainw->merge, FALSE);

  mainw->xdelete = gtk_image_menu_item_new_with_mnemonic(_("_Delete Selection"));
  gtk_widget_show (mainw->xdelete);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->xdelete);
  gtk_widget_set_sensitive (mainw->xdelete, FALSE);

  image1334 = gtk_image_new_from_stock ("gtk-delete", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image1334);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->xdelete), image1334);

  gtk_widget_add_accelerator (mainw->xdelete, "activate", mainw->accel_group,
                              GDK_d, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  separator5 = gtk_menu_item_new ();
  gtk_widget_show (separator5);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), separator5);
  gtk_widget_set_sensitive (separator5, FALSE);

  mainw->ccpd_sound = gtk_check_menu_item_new_with_mnemonic (_("Decouple _Video from Audio"));
  gtk_widget_show (mainw->ccpd_sound);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->ccpd_sound);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->ccpd_sound),!mainw->ccpd_with_sound);

  separator55 = gtk_menu_item_new ();
  gtk_widget_show (separator55);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), separator55);
  gtk_widget_set_sensitive (separator55, FALSE);

  mainw->select_submenu = gtk_menu_item_new_with_mnemonic (_("_Select..."));
  gtk_widget_show (mainw->select_submenu);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->select_submenu);
  gtk_widget_set_sensitive(mainw->select_submenu,FALSE);

  select_submenu_menu=gtk_menu_new();

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->select_submenu), select_submenu_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(select_submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->select_all = gtk_menu_item_new_with_mnemonic (_("Select _All Frames"));
  gtk_widget_show (mainw->select_all);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_all);

  gtk_widget_add_accelerator (mainw->select_all, "activate", mainw->accel_group,
                              GDK_a, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->select_start_only = gtk_image_menu_item_new_with_mnemonic (_("_Start Frame Only"));
  gtk_widget_show (mainw->select_start_only);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_start_only);

  gtk_widget_add_accelerator (mainw->select_start_only, "activate", mainw->accel_group,
			      GDK_Home, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);

  mainw->select_end_only = gtk_image_menu_item_new_with_mnemonic (_("_End Frame Only"));
  gtk_widget_show (mainw->select_end_only);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_end_only);
  gtk_widget_add_accelerator (mainw->select_end_only, "activate", mainw->accel_group,
			      GDK_End, GDK_CONTROL_MASK,
			      GTK_ACCEL_VISIBLE);


  separatormenuitem110 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem110);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), separatormenuitem110);
  gtk_widget_set_sensitive (separatormenuitem110,FALSE);
    
  mainw->select_from_start = gtk_image_menu_item_new_with_mnemonic (_("Select from _First Frame"));
  gtk_widget_show (mainw->select_from_start);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_from_start);

  mainw->select_to_end = gtk_image_menu_item_new_with_mnemonic (_("Select to _Last Frame"));
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_to_end);
  gtk_widget_show (mainw->select_to_end);

  mainw->select_new = gtk_image_menu_item_new_with_mnemonic (_("Select Last Insertion/_Merge"));
  gtk_widget_show (mainw->select_new);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_new);

  mainw->select_last = gtk_image_menu_item_new_with_mnemonic (_("Select Last _Effect"));
  gtk_widget_show (mainw->select_last);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_last);

  mainw->select_invert = gtk_image_menu_item_new_with_mnemonic (_("_Invert Selection"));
  gtk_widget_show (mainw->select_invert);
  gtk_container_add (GTK_CONTAINER (select_submenu_menu), mainw->select_invert);

  gtk_widget_add_accelerator (mainw->select_invert, "activate", mainw->accel_group,
                              GDK_slash, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->lock_selwidth = gtk_check_menu_item_new_with_mnemonic (_("_Lock Selection Width"));
  gtk_widget_show (mainw->lock_selwidth);
  gtk_container_add (GTK_CONTAINER (menuitem12_menu), mainw->lock_selwidth);
  gtk_widget_set_sensitive(mainw->lock_selwidth,FALSE);

  menuitem13 = gtk_menu_item_new_with_mnemonic (_ ("_Play"));
  gtk_widget_show (menuitem13);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), menuitem13);
  
  menuitem13_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem13), menuitem13_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem13_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->playall = gtk_image_menu_item_new_with_mnemonic (_("_Play All"));
  gtk_widget_add_accelerator (mainw->playall, "activate", mainw->accel_group,
                              GDK_p, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (mainw->playall);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->playall);
  gtk_widget_set_sensitive (mainw->playall, FALSE);

  image335 = gtk_image_new_from_stock ("gtk-refresh", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image335);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->playall), image335);

  mainw->playsel = gtk_image_menu_item_new_with_mnemonic (_("Pla_y Selection"));
  gtk_widget_add_accelerator (mainw->playsel, "activate", mainw->accel_group,
                              GDK_y, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (mainw->playsel);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->playsel);
  gtk_widget_set_sensitive (mainw->playsel, FALSE);

  mainw->playclip = gtk_image_menu_item_new_with_mnemonic (_("Play _Clipboard"));
  gtk_widget_show (mainw->playclip);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->playclip);
  gtk_widget_set_sensitive (mainw->playclip, FALSE);

  gtk_widget_add_accelerator (mainw->playclip, "activate", mainw->accel_group,
                              GDK_c, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);


  image336 = gtk_image_new_from_stock ("gtk-go-forward", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image336);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->playsel), image336);
  image355 = gtk_image_new_from_stock ("gtk-go-forward", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image355);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->playclip), image355);

  mainw->stop = gtk_image_menu_item_new_with_mnemonic (_("_Stop"));
  gtk_widget_show (mainw->stop);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->stop);
  gtk_widget_set_sensitive (mainw->stop, FALSE);
  gtk_widget_add_accelerator (mainw->stop, "activate", mainw->accel_group,
                              GDK_q, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);


  image337 = gtk_image_new_from_stock ("gtk-stop", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image337);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->stop), image337);

  mainw->rewind = gtk_image_menu_item_new_with_mnemonic (_("Re_wind"));
  gtk_widget_show (mainw->rewind);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->rewind);
  gtk_widget_set_sensitive (mainw->rewind, FALSE);

  image1337 = gtk_image_new_from_stock ("gtk-back", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image1337);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->rewind), image1337);

  gtk_widget_add_accelerator (mainw->rewind, "activate", mainw->accel_group,
                              GDK_w, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->record_perf = gtk_check_menu_item_new_with_mnemonic("");

  disable_record();

  gtk_widget_add_accelerator (mainw->record_perf, "activate", mainw->accel_group,
			      GDK_r, (GdkModifierType)0,
			      GTK_ACCEL_VISIBLE);

  gtk_widget_show (mainw->record_perf);

  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->record_perf);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->record_perf),FALSE);

  separator8 = gtk_menu_item_new ();
  gtk_widget_show (separator8);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), separator8);
  gtk_widget_set_sensitive (separator8, FALSE);

  mainw->full_screen = gtk_check_menu_item_new_with_mnemonic (_("_Full Screen"));
  gtk_widget_show (mainw->full_screen);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->full_screen);
  

  gtk_widget_add_accelerator (mainw->full_screen, "activate", mainw->accel_group,
                              GDK_f, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->dsize = gtk_check_menu_item_new_with_mnemonic (_("_Double Size"));
  gtk_widget_show (mainw->dsize);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->dsize);

  gtk_widget_add_accelerator (mainw->dsize, "activate", mainw->accel_group,
                              GDK_d, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->sepwin = gtk_check_menu_item_new_with_mnemonic (_("Play in _Separate Window"));
  gtk_widget_show (mainw->sepwin);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->sepwin);

  gtk_widget_add_accelerator (mainw->sepwin, "activate", mainw->accel_group,
                              GDK_s, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);


  mainw->fade = gtk_check_menu_item_new_with_mnemonic (_("_Blank Background"));
  gtk_widget_add_accelerator (mainw->fade, "activate", mainw->accel_group,
                              GDK_b, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (mainw->fade);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->fade);

  mainw->loop_video = gtk_check_menu_item_new_with_mnemonic (_("(Auto)_loop Video (to fit audio track)"));
  gtk_widget_show (mainw->loop_video);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->loop_video);
  gtk_widget_set_sensitive (mainw->loop_video, FALSE);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->loop_video),mainw->loop);
  gtk_widget_add_accelerator (mainw->loop_video, "activate", mainw->accel_group,
                              GDK_l, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->loop_continue = gtk_check_menu_item_new_with_mnemonic (_("L_oop Continuously"));
  gtk_widget_show (mainw->loop_continue);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->loop_continue);
  gtk_widget_set_sensitive (mainw->loop_continue, FALSE);

  gtk_widget_add_accelerator (mainw->loop_continue, "activate", mainw->accel_group,
                              GDK_o, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->loop_ping_pong = gtk_check_menu_item_new_with_mnemonic (_("Pin_g Pong Loops"));
  gtk_widget_show (mainw->loop_ping_pong);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->loop_ping_pong);

  gtk_widget_add_accelerator (mainw->loop_ping_pong, "activate", mainw->accel_group,
                              GDK_g, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  mainw->mute_audio = gtk_check_menu_item_new_with_mnemonic (_("_Mute"));
  gtk_widget_show (mainw->mute_audio);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->mute_audio);
  gtk_widget_set_sensitive (mainw->mute_audio, FALSE);

  gtk_widget_add_accelerator (mainw->mute_audio, "activate", mainw->accel_group,
                              GDK_z, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  separator49 = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), separator49);
  gtk_widget_set_sensitive (separator49, FALSE);
  gtk_widget_show (separator49);

  mainw->sticky = gtk_check_menu_item_new_with_mnemonic (_("Separate Window 'S_ticky' Mode"));
  gtk_widget_show (mainw->sticky);

  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->sticky);
  if (capable->smog_version_correct&&prefs->sepwin_type==1) {
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->sticky),TRUE);
  }

  mainw->showfct = gtk_check_menu_item_new_with_mnemonic (_("S_how Frame Counter"));
  gtk_widget_show (mainw->showfct);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->showfct);

  gtk_widget_add_accelerator (mainw->showfct, "activate", mainw->accel_group,
                              GDK_h, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->showfct),capable->smog_version_correct&&prefs->show_framecount);

  mainw->showsubs = gtk_check_menu_item_new_with_mnemonic (_("Show Subtitles"));
  gtk_widget_show (mainw->showsubs);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->showsubs);

  gtk_widget_add_accelerator (mainw->showsubs, "activate", mainw->accel_group,
                              GDK_v, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->showsubs),prefs->show_subtitles);

  mainw->letter = gtk_check_menu_item_new_with_mnemonic (_("Letterbox mode"));
  gtk_widget_show (mainw->letter);
  gtk_container_add (GTK_CONTAINER (menuitem13_menu), mainw->letter);

  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->letter),prefs->letterbox);

  effects = gtk_menu_item_new_with_mnemonic (_ ("Effect_s"));
  gtk_widget_show (effects);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), effects);
  gtk_widget_set_tooltip_text( effects,(_ ("Effects are applied to the current selection.")));

  // the dynamic effects menu
  mainw->effects_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (effects), mainw->effects_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->effects_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->custom_effects_menu=NULL;

  mainw->run_test_rfx_submenu=gtk_menu_item_new_with_mnemonic (_("_Run Test Rendered Effect/Tool/Generator..."));
  mainw->run_test_rfx_menu=NULL;

  mainw->num_rendered_effects_builtin=mainw->num_rendered_effects_custom=mainw->num_rendered_effects_test=0;

  tools = gtk_menu_item_new_with_mnemonic (_("_Tools"));
  gtk_widget_show (tools);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), tools);
  gtk_widget_set_tooltip_text( tools,(_ ("Tools are applied to complete clips.")));

  mainw->tools_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (tools), mainw->tools_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->tools_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->rev_clipboard = gtk_menu_item_new_with_mnemonic (_("_Reverse Clipboard"));
  gtk_widget_show (mainw->rev_clipboard);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->rev_clipboard);
  gtk_widget_set_sensitive (mainw->rev_clipboard, FALSE);

  gtk_widget_add_accelerator (mainw->rev_clipboard, "activate", mainw->accel_group,
                              GDK_x, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  mainw->change_speed = gtk_menu_item_new_with_mnemonic (_("_Change Playback/Save Speed..."));
  gtk_widget_show (mainw->change_speed);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->change_speed);
  gtk_widget_set_sensitive (mainw->change_speed, FALSE);

  mainw->resample_video = gtk_menu_item_new_with_mnemonic (_("Resample _Video to New Frame Rate..."));
  gtk_widget_show (mainw->resample_video);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->resample_video);
  gtk_widget_set_sensitive (mainw->resample_video, FALSE);

  mainw->utilities_menu=NULL;
  mainw->utilities_submenu = gtk_menu_item_new_with_mnemonic (_("_Utilities"));
  gtk_widget_show (mainw->utilities_submenu);

  mainw->custom_utilities_menu=NULL;

  mainw->custom_tools_submenu = gtk_menu_item_new_with_mnemonic (_("Custom _Tools"));

  mainw->custom_tools_separator = gtk_menu_item_new ();
  gtk_widget_set_sensitive (mainw->custom_tools_separator, FALSE);

  mainw->gens_menu=NULL;
  mainw->gens_submenu = gtk_menu_item_new_with_mnemonic (_("_Generate"));
  gtk_widget_show (mainw->gens_submenu);

  // add RFX plugins
  mainw->rte_separator=NULL;
  mainw->custom_gens_menu=NULL;
  mainw->rendered_fx=NULL;
  mainw->custom_tools_menu=NULL;

  if (!mainw->foreign&&capable->smog_version_correct) {
    splash_msg(_("Loading rendered effect plugins..."),.2);
    add_rfx_effects();
    splash_msg(_("Starting GUI..."),.4);
  }

  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->utilities_submenu);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->custom_tools_separator);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->custom_tools_submenu);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->gens_submenu);

  separator26 = gtk_menu_item_new ();
  gtk_widget_show (separator26);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), separator26);
  gtk_widget_set_sensitive (separator26, FALSE);

  mainw->load_subs = gtk_menu_item_new_with_mnemonic (_("Load _Subtitles from File..."));
  gtk_widget_show (mainw->load_subs);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->load_subs);
  gtk_widget_set_sensitive (mainw->load_subs, FALSE);

  mainw->erase_subs = gtk_menu_item_new_with_mnemonic (_("Erase subtitles"));
  gtk_widget_show (mainw->erase_subs);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->erase_subs);
  gtk_widget_set_sensitive (mainw->erase_subs, FALSE);

  separator26 = gtk_menu_item_new ();
  gtk_widget_show (separator26);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), separator26);
  gtk_widget_set_sensitive (separator26, FALSE);

  mainw->capture = gtk_menu_item_new_with_mnemonic (_("Capture _External Window... "));
  gtk_widget_show (mainw->capture);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->capture);

  separator6 = gtk_menu_item_new ();
  gtk_widget_show (separator6);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), separator6);
  gtk_widget_set_sensitive (separator6, FALSE);

  mainw->preferences = gtk_image_menu_item_new_with_mnemonic (_("_Preferences..."));
  gtk_widget_show (mainw->preferences);
  gtk_container_add (GTK_CONTAINER (mainw->tools_menu), mainw->preferences);
  gtk_widget_add_accelerator (mainw->preferences, "activate", mainw->accel_group,
                              GDK_p, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);

  image346 = gtk_image_new_from_stock ("gtk-preferences", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image346);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->preferences), image346);

  audio = gtk_menu_item_new_with_mnemonic (_("_Audio"));
  gtk_widget_show (audio);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), audio);

  audio_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (audio), audio_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(audio_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->load_audio = gtk_menu_item_new_with_mnemonic (_("Load _New Audio for Clip..."));

  gtk_widget_show (mainw->load_audio);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->load_audio);
  gtk_widget_set_sensitive (mainw->load_audio, FALSE);

  mainw->load_cdtrack = gtk_menu_item_new_with_mnemonic (_("Load _CD Track..."));
  mainw->eject_cd = gtk_menu_item_new_with_mnemonic (_("E_ject CD"));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->load_cdtrack);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->eject_cd);

  if (capable->smog_version_correct) {
    if (!(capable->has_cdda2wav&&strlen (prefs->cdplay_device))) {
      gtk_widget_set_sensitive (mainw->load_cdtrack, FALSE);
      gtk_widget_set_sensitive (mainw->eject_cd, FALSE);
    }
  }

  gtk_widget_show (mainw->eject_cd);
  gtk_widget_show (mainw->load_cdtrack);

  mainw->recaudio_submenu = gtk_menu_item_new_with_mnemonic (_("Record E_xternal Audio..."));
  if ((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio)) gtk_widget_show (mainw->recaudio_submenu);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->recaudio_submenu);

  submenu_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->recaudio_submenu), submenu_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show (mainw->recaudio_submenu);

  mainw->recaudio_clip = gtk_menu_item_new_with_mnemonic (_("to New _Clip..."));
  gtk_widget_show (mainw->recaudio_clip);
  gtk_container_add (GTK_CONTAINER (submenu_menu), mainw->recaudio_clip);

  mainw->recaudio_sel = gtk_menu_item_new_with_mnemonic (_("to _Selection"));
  gtk_widget_show (mainw->recaudio_sel);
  gtk_container_add (GTK_CONTAINER (submenu_menu), mainw->recaudio_sel);
  gtk_widget_set_sensitive(mainw->recaudio_sel,FALSE);

  separator9 = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (audio_menu), separator9);
  gtk_widget_set_sensitive (separator9, FALSE);
  gtk_widget_show (separator9);
  
  mainw->fade_aud_in = gtk_menu_item_new_with_mnemonic (_("Fade Audio _In..."));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->fade_aud_in);
  gtk_widget_show (mainw->fade_aud_in);

  mainw->fade_aud_out = gtk_menu_item_new_with_mnemonic (_("Fade Audio _Out..."));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->fade_aud_out);
  gtk_widget_show (mainw->fade_aud_out);

  gtk_widget_set_sensitive (mainw->fade_aud_in, FALSE);
  gtk_widget_set_sensitive (mainw->fade_aud_out, FALSE);

  separator9 = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (audio_menu), separator9);
  gtk_widget_set_sensitive (separator9, FALSE);
  gtk_widget_show (separator9);

  mainw->export_submenu = gtk_menu_item_new_with_mnemonic (_("_Export Audio..."));
  gtk_widget_show (mainw->export_submenu);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->export_submenu);
  gtk_widget_set_sensitive(mainw->export_submenu,FALSE);

  export_submenu_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->export_submenu), export_submenu_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(export_submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show (mainw->export_submenu);

  mainw->export_selaudio = gtk_menu_item_new_with_mnemonic (_("Export _Selected Audio..."));
  gtk_widget_show (mainw->export_selaudio);
  gtk_container_add (GTK_CONTAINER (export_submenu_menu), mainw->export_selaudio);

  mainw->export_allaudio = gtk_menu_item_new_with_mnemonic (_("Export _All Audio..."));
  gtk_widget_show (mainw->export_allaudio);
  gtk_container_add (GTK_CONTAINER (export_submenu_menu), mainw->export_allaudio);

  mainw->append_audio = gtk_menu_item_new_with_mnemonic (_("_Append Audio..."));
  gtk_widget_show (mainw->append_audio);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->append_audio);
  gtk_widget_set_sensitive (mainw->append_audio, FALSE);

  mainw->trim_submenu = gtk_menu_item_new_with_mnemonic (_("_Trim/Pad Audio..."));
  gtk_widget_show (mainw->trim_submenu);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->trim_submenu);
  gtk_widget_set_sensitive(mainw->trim_submenu,FALSE);

  trimaudio_submenu_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->trim_submenu), trimaudio_submenu_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(trimaudio_submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_widget_show (mainw->trim_submenu);
  mainw->trim_audio = gtk_menu_item_new_with_mnemonic (_("Trim/Pad Audio to _Selection"));
  gtk_widget_show (mainw->trim_audio);
  gtk_container_add (GTK_CONTAINER (trimaudio_submenu_menu), mainw->trim_audio);
  gtk_widget_set_sensitive (mainw->trim_audio, FALSE);

  mainw->trim_to_pstart = gtk_menu_item_new_with_mnemonic (_("Trim/Pad Audio from Beginning to _Play Start"));
  gtk_widget_show (mainw->trim_to_pstart);
  gtk_container_add (GTK_CONTAINER (trimaudio_submenu_menu), mainw->trim_to_pstart);
  gtk_widget_set_sensitive (mainw->trim_to_pstart, FALSE);

  mainw->delaudio_submenu = gtk_menu_item_new_with_mnemonic (_("_Delete Audio..."));
  gtk_widget_show (mainw->delaudio_submenu);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->delaudio_submenu);
  gtk_widget_set_sensitive(mainw->delaudio_submenu,FALSE);

  delaudio_submenu_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->delaudio_submenu), delaudio_submenu_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(delaudio_submenu_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show (mainw->delaudio_submenu);

  mainw->delsel_audio = gtk_menu_item_new_with_mnemonic (_("Delete _Selected Audio"));
  gtk_widget_show (mainw->delsel_audio);
  gtk_container_add (GTK_CONTAINER (delaudio_submenu_menu), mainw->delsel_audio);

  mainw->delall_audio = gtk_menu_item_new_with_mnemonic (_("Delete _All Audio"));
  gtk_widget_show (mainw->delall_audio);
  gtk_container_add (GTK_CONTAINER (delaudio_submenu_menu), mainw->delall_audio);

  mainw->ins_silence = gtk_menu_item_new_with_mnemonic (_("Insert _Silence in Selection"));
  gtk_widget_show (mainw->ins_silence);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->ins_silence);
  gtk_widget_set_sensitive (mainw->ins_silence, FALSE);

  mainw->resample_audio = gtk_menu_item_new_with_mnemonic (_("_Resample Audio..."));
  gtk_widget_show (mainw->resample_audio);
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->resample_audio);
  gtk_widget_set_sensitive (mainw->resample_audio, FALSE);

  separator19 = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (audio_menu), separator19);
  gtk_widget_set_sensitive (separator19, FALSE);
  gtk_widget_show (separator19);

  mainw->xmms_play_audio = gtk_menu_item_new_with_mnemonic (_("Play _Music Using xmms..."));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->xmms_play_audio);
  gtk_widget_set_sensitive (mainw->xmms_play_audio, TRUE);

  mainw->xmms_random_audio = gtk_menu_item_new_with_mnemonic (_("Random Music _Using xmms..."));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->xmms_random_audio);
  gtk_widget_set_sensitive (mainw->xmms_random_audio, TRUE);

  mainw->xmms_stop_audio = gtk_menu_item_new_with_mnemonic (_("Stop xmms _Playing"));
  gtk_container_add (GTK_CONTAINER (audio_menu), mainw->xmms_stop_audio);
  gtk_widget_set_sensitive (mainw->xmms_stop_audio, TRUE);
  
  gtk_widget_show (mainw->xmms_stop_audio);
  gtk_widget_show (mainw->xmms_play_audio);
  gtk_widget_show (mainw->xmms_random_audio);
  gtk_widget_show (separator9);
  
  if (!capable->has_xmms) {
    gtk_widget_set_sensitive (mainw->xmms_stop_audio,FALSE);
    gtk_widget_set_sensitive (mainw->xmms_play_audio,FALSE);
    gtk_widget_set_sensitive (mainw->xmms_random_audio,FALSE);
  }

  info = gtk_menu_item_new_with_mnemonic (_("_Info"));
  gtk_widget_show (info);
  gtk_container_add (GTK_CONTAINER(mainw->menubar), info);

  info_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (info), info_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(info_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->show_file_info = gtk_image_menu_item_new_with_mnemonic (_("Show Clip _Info"));
  gtk_widget_add_accelerator (mainw->show_file_info, "activate", mainw->accel_group,
                              GDK_i, (GdkModifierType)0,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_show (mainw->show_file_info);
  gtk_container_add (GTK_CONTAINER (info_menu), mainw->show_file_info);
  gtk_widget_set_sensitive (mainw->show_file_info, FALSE);

  mainw->show_file_comments = gtk_image_menu_item_new_with_mnemonic (_("Show/_Edit File Comments"));
  gtk_widget_show (mainw->show_file_comments);
  gtk_container_add (GTK_CONTAINER (info_menu), mainw->show_file_comments);
  gtk_widget_set_sensitive (mainw->show_file_comments, FALSE);

  mainw->show_clipboard_info = gtk_image_menu_item_new_with_mnemonic (_("Show _Clipboard Info"));
  gtk_widget_show (mainw->show_clipboard_info);
  gtk_container_add (GTK_CONTAINER (info_menu), mainw->show_clipboard_info);
  gtk_widget_set_sensitive (mainw->show_clipboard_info, FALSE);

  image347 = gtk_image_new_from_stock ("gtk-dialog-info", GTK_ICON_SIZE_MENU);
  gtk_widget_show (image347);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->show_file_info), image347);

  mainw->show_messages = gtk_image_menu_item_new_with_mnemonic (_("Show _Messages"));
  gtk_widget_show (mainw->show_messages);
  gtk_container_add (GTK_CONTAINER (info_menu), mainw->show_messages);

  mainw->show_layout_errors = gtk_image_menu_item_new_with_mnemonic (_("Show _Layout Errors"));
  gtk_widget_show (mainw->show_layout_errors);
  gtk_container_add (GTK_CONTAINER (info_menu), mainw->show_layout_errors);
  gtk_widget_set_sensitive (mainw->show_layout_errors, FALSE);

  win = gtk_menu_item_new_with_mnemonic (_("_Clips"));
  gtk_widget_show (win);
  gtk_container_add (GTK_CONTAINER(mainw->menubar), win);

  mainw->winmenu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (win), mainw->winmenu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->winmenu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->rename = gtk_image_menu_item_new_with_mnemonic (_("_Rename Current Clip in Menu..."));
  gtk_widget_show (mainw->rename);
  gtk_container_add (GTK_CONTAINER (mainw->winmenu), mainw->rename);
  gtk_widget_set_sensitive (mainw->rename, FALSE);

  separator10 = gtk_menu_item_new ();
  gtk_widget_show (separator10);
  gtk_container_add (GTK_CONTAINER (mainw->winmenu), separator10);
  gtk_widget_set_sensitive (separator10, FALSE);

  menuitemsep = gtk_menu_item_new_with_label ("|");
  gtk_widget_show (menuitemsep);
  gtk_container_add (GTK_CONTAINER(mainw->menubar), menuitemsep);
  gtk_widget_set_sensitive (menuitemsep,FALSE);

  advanced = gtk_menu_item_new_with_mnemonic (_("A_dvanced"));
  gtk_widget_show (advanced);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), advanced);

  advanced_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (advanced), advanced_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(advanced_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show(advanced_menu);

  rfx_submenu = gtk_menu_item_new_with_mnemonic (_ ("_RFX Effects/Tools/Utilities"));
  gtk_widget_show(rfx_submenu);
  gtk_container_add (GTK_CONTAINER (advanced_menu), rfx_submenu);

  rfx_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (rfx_submenu), rfx_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(rfx_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show(rfx_menu);

  new_test_rfx=gtk_menu_item_new_with_mnemonic (_ ("_New Test RFX Script..."));
  gtk_widget_show(new_test_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), new_test_rfx);

  copy_rfx=gtk_menu_item_new_with_mnemonic (_ ("_Copy RFX Script to Test..."));
  gtk_widget_show(copy_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), copy_rfx);

  mainw->edit_test_rfx=gtk_menu_item_new_with_mnemonic (_ ("_Edit Test RFX Script..."));
  gtk_widget_show(mainw->edit_test_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->edit_test_rfx);

  mainw->rename_test_rfx=gtk_menu_item_new_with_mnemonic (_ ("Rena_me Test RFX Script..."));
  gtk_widget_show(mainw->rename_test_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->rename_test_rfx);

  mainw->delete_test_rfx=gtk_menu_item_new_with_mnemonic (_ ("_Delete Test RFX Script..."));
  gtk_widget_show(mainw->delete_test_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->delete_test_rfx);

  separator6 = gtk_menu_item_new ();
  gtk_widget_show (separator6);
  gtk_container_add (GTK_CONTAINER (rfx_menu), separator6);
  gtk_widget_set_sensitive (separator6, FALSE);

  gtk_widget_show(mainw->run_test_rfx_submenu);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->run_test_rfx_submenu);
  
  mainw->promote_test_rfx=gtk_menu_item_new_with_mnemonic (_ ("_Promote Test Rendered Effect/Tool/Generator..."));
  gtk_widget_show(mainw->promote_test_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->promote_test_rfx);

  separator6 = gtk_menu_item_new ();
  gtk_widget_show (separator6);
  gtk_container_add (GTK_CONTAINER (rfx_menu), separator6);
  gtk_widget_set_sensitive (separator6, FALSE);

  import_custom_rfx=gtk_menu_item_new_with_mnemonic (_ ("_Import Custom RFX script..."));
  gtk_widget_show(import_custom_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), import_custom_rfx);

  mainw->export_custom_rfx=gtk_menu_item_new_with_mnemonic (_ ("E_xport Custom RFX script..."));
  gtk_widget_show(mainw->export_custom_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->export_custom_rfx);

  mainw->delete_custom_rfx=gtk_menu_item_new_with_mnemonic (_ ("De_lete Custom RFX Script..."));
  gtk_widget_show(mainw->delete_custom_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), mainw->delete_custom_rfx);

  separator6 = gtk_menu_item_new ();
  gtk_widget_show (separator6);
  gtk_container_add (GTK_CONTAINER (rfx_menu), separator6);
  gtk_widget_set_sensitive (separator6, FALSE);

  rebuild_rfx=gtk_menu_item_new_with_mnemonic (_ ("Re_build all RFX plugins"));
  gtk_widget_show(rebuild_rfx);
  gtk_container_add (GTK_CONTAINER (rfx_menu), rebuild_rfx);


  if (mainw->num_rendered_effects_custom>0) {
    gtk_widget_set_sensitive(mainw->delete_custom_rfx,TRUE);
    gtk_widget_set_sensitive(mainw->export_custom_rfx,TRUE);
  }
  else {
    gtk_widget_set_sensitive(mainw->delete_custom_rfx,FALSE);
    gtk_widget_set_sensitive(mainw->export_custom_rfx,FALSE);
  }
  
  if (mainw->num_rendered_effects_test>0) {
    gtk_widget_set_sensitive(mainw->run_test_rfx_submenu,TRUE);
    gtk_widget_set_sensitive(mainw->promote_test_rfx,TRUE);
    gtk_widget_set_sensitive(mainw->delete_test_rfx,TRUE);
    gtk_widget_set_sensitive(mainw->rename_test_rfx,TRUE);
    gtk_widget_set_sensitive(mainw->edit_test_rfx,TRUE);
  }
  else {
    gtk_widget_set_sensitive(mainw->run_test_rfx_submenu,FALSE);
    gtk_widget_set_sensitive(mainw->promote_test_rfx,FALSE);
    gtk_widget_set_sensitive(mainw->delete_test_rfx,FALSE);
    gtk_widget_set_sensitive(mainw->rename_test_rfx,FALSE);
    gtk_widget_set_sensitive(mainw->edit_test_rfx,FALSE);
  }
  
  mainw->open_lives2lives = gtk_menu_item_new_with_mnemonic (_("Receive _LiVES stream from..."));

  separator19 = gtk_menu_item_new ();
  gtk_container_add (GTK_CONTAINER (advanced_menu), separator19);
  gtk_widget_set_sensitive (separator19, FALSE);
  gtk_widget_show (separator19);
  gtk_widget_show (mainw->open_lives2lives);

  mainw->send_lives2lives = gtk_menu_item_new_with_mnemonic (_("_Send LiVES stream to..."));

  gtk_widget_show (mainw->send_lives2lives);
  gtk_container_add (GTK_CONTAINER (advanced_menu), mainw->send_lives2lives);
  gtk_container_add (GTK_CONTAINER (advanced_menu), mainw->open_lives2lives);

  if (capable->smog_version_correct) {
    mainw->open_yuv4m = gtk_menu_item_new_with_mnemonic ((tmp=g_strdup_printf (_("Open _yuv4mpeg stream on %sstream.yuv..."),prefs->tmpdir)));
    g_free(tmp);
#ifdef HAVE_YUV4MPEG
    separator19 = gtk_menu_item_new ();
    gtk_container_add (GTK_CONTAINER (advanced_menu), separator19);
    gtk_widget_set_sensitive (separator19, FALSE);
    gtk_widget_show (separator19);
    gtk_widget_show (mainw->open_yuv4m);
    gtk_container_add (GTK_CONTAINER (advanced_menu), mainw->open_yuv4m);

    // TODO - apply a deinterlace filter to yuv4mpeg frames
    /*    mainw->yuv4m_deint = gtk_check_menu_item_new_with_mnemonic (_("_Deinterlace yuv4mpeg frames"));
    gtk_widget_show (mainw->yuv4m_deint);
    gtk_container_add (GTK_CONTAINER (advance_menu), mainw->yuv4m_deint);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->yu4m_deint),TRUE);*/
#endif
  }


  separatormenuitem99 = gtk_menu_item_new ();
  gtk_widget_show (separatormenuitem99);
  gtk_container_add (GTK_CONTAINER (advanced_menu), separatormenuitem99);
  gtk_widget_set_sensitive (separatormenuitem99, FALSE);

  mainw->import_proj = gtk_menu_item_new_with_mnemonic (_("_Import Project (.lv2)..."));
  gtk_container_add (GTK_CONTAINER (advanced_menu), mainw->import_proj);
  gtk_widget_show (mainw->import_proj);

  mainw->export_proj = gtk_menu_item_new_with_mnemonic (_("E_xport Project (.lv2)..."));
  gtk_container_add (GTK_CONTAINER (advanced_menu), mainw->export_proj);
  gtk_widget_show (mainw->export_proj);
  gtk_widget_set_sensitive (mainw->export_proj, FALSE);

  mainw->vj_menu = gtk_menu_item_new_with_mnemonic (_("_VJ"));
  gtk_widget_show (mainw->vj_menu);
  gtk_container_add (GTK_CONTAINER(mainw->menubar), mainw->vj_menu);

  vj_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->vj_menu), vj_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(vj_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  assign_rte_keys = gtk_menu_item_new_with_mnemonic (_("Real Time _Effect Mapping"));
  gtk_widget_show (assign_rte_keys);
  gtk_container_add (GTK_CONTAINER (vj_menu), assign_rte_keys);
  gtk_widget_add_accelerator (assign_rte_keys, "activate", mainw->accel_group,
                              GDK_v, GDK_CONTROL_MASK,
                              GTK_ACCEL_VISIBLE);
  gtk_widget_set_tooltip_text( assign_rte_keys,(_ ("Bind real time effects to ctrl-number keys.")));

  mainw->rte_defs_menu=gtk_menu_item_new_with_mnemonic (_("Set Real Time Effect _Defaults"));
  gtk_container_add (GTK_CONTAINER (vj_menu), mainw->rte_defs_menu);
  gtk_widget_set_tooltip_text( mainw->rte_defs_menu,(_ ("Set default parameter values for real time effects.")));

  mainw->rte_defs=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->rte_defs_menu), mainw->rte_defs);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->rte_defs, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_widget_show (mainw->rte_defs_menu);
  gtk_widget_show (mainw->rte_defs);

  mainw->save_rte_defs=gtk_menu_item_new_with_mnemonic (_("Save Real Time Effect _Defaults"));
  gtk_container_add (GTK_CONTAINER (vj_menu), mainw->save_rte_defs);
  gtk_widget_show (mainw->save_rte_defs);
  gtk_widget_set_tooltip_text( mainw->save_rte_defs,(_ ("Save real time effect defaults so they will be restored each time you use LiVES.")));

  separator88 = gtk_menu_item_new ();
  gtk_widget_show (separator88);
  gtk_container_add (GTK_CONTAINER (vj_menu), separator88);
  gtk_widget_set_sensitive (separator88, FALSE);

  mainw->vj_reset=gtk_menu_item_new_with_mnemonic (_("_Reset all playback speeds and positions"));
  gtk_container_add (GTK_CONTAINER (vj_menu), mainw->vj_reset);
  gtk_widget_show (mainw->vj_reset);
  gtk_widget_set_tooltip_text( mainw->vj_reset,(_ ("Reset all playback positions to frame 1, and reset all playback frame rates.")));

  midi_submenu = gtk_menu_item_new_with_mnemonic (_ ("_MIDI/joystick interface"));

#ifdef ENABLE_OSC
  gtk_widget_show(midi_submenu);
  gtk_container_add (GTK_CONTAINER (vj_menu), midi_submenu);
#endif

  midi_menu=gtk_menu_new();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (midi_submenu), midi_menu);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(midi_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }
  gtk_widget_show(midi_menu);

  midi_learn = gtk_menu_item_new_with_mnemonic (_("_MIDI/joystick learner..."));

  gtk_widget_show (midi_learn);
  gtk_container_add (GTK_CONTAINER (midi_menu), midi_learn);

  midi_save = gtk_menu_item_new_with_mnemonic (_("_Save device mapping..."));

  gtk_widget_show (midi_save);
  gtk_container_add (GTK_CONTAINER (midi_menu), midi_save);


  midi_load = gtk_menu_item_new_with_mnemonic (_("_Load device mapping..."));

  gtk_widget_show (midi_load);
  gtk_container_add (GTK_CONTAINER (midi_menu), midi_load);


  separator88 = gtk_menu_item_new ();
  gtk_widget_show (separator88);
  gtk_container_add (GTK_CONTAINER (vj_menu), separator88);
  gtk_widget_set_sensitive (separator88, FALSE);

  mainw->vj_show_keys = gtk_menu_item_new_with_mnemonic (_("Show VJ _Keys"));
  gtk_widget_show (mainw->vj_show_keys);
  gtk_container_add (GTK_CONTAINER (vj_menu), mainw->vj_show_keys);

  mainw->toys = gtk_menu_item_new_with_mnemonic (_("To_ys"));
  gtk_widget_show (mainw->toys);
  gtk_container_add (GTK_CONTAINER(mainw->menubar), mainw->toys);

  toys_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (mainw->toys), toys_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(toys_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  mainw->toy_none = gtk_check_menu_item_new_with_mnemonic (_("_None"));
  gtk_widget_show (mainw->toy_none);
  gtk_container_add (GTK_CONTAINER (toys_menu), mainw->toy_none);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->toy_none),TRUE);

  separator27 = gtk_menu_item_new ();
  gtk_widget_show (separator27);
  gtk_container_add (GTK_CONTAINER (toys_menu), separator27);
  gtk_widget_set_sensitive (separator27, FALSE);

  mainw->toy_autolives = gtk_check_menu_item_new_with_mnemonic (_("_Autolives"));
  gtk_widget_show (mainw->toy_autolives);
  gtk_container_add (GTK_CONTAINER (toys_menu), mainw->toy_autolives);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->toy_autolives),FALSE);

  mainw->toy_random_frames = gtk_check_menu_item_new_with_mnemonic (_("_Mad Frames"));
  gtk_widget_show (mainw->toy_random_frames);
  gtk_container_add (GTK_CONTAINER (toys_menu), mainw->toy_random_frames);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->toy_random_frames),FALSE);

  mainw->toy_tv = gtk_check_menu_item_new_with_mnemonic (_("_LiVES TV (broadband)"));

  gtk_container_add (GTK_CONTAINER (toys_menu), mainw->toy_tv);

#ifdef LIVES_TV_CHANNEL1
  gtk_widget_show (mainw->toy_tv);
#else
  gtk_widget_hide (mainw->toy_tv);
# endif

  menuitem14 = gtk_menu_item_new_with_mnemonic (_("_Help"));
  gtk_widget_show (menuitem14);
  gtk_container_add (GTK_CONTAINER (mainw->menubar), menuitem14);

  menuitem14_menu = gtk_menu_new ();
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem14), menuitem14_menu);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(menuitem14_menu, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  show_manual = gtk_menu_item_new_with_mnemonic (_("_Manual (opens in browser)"));
  gtk_widget_show (show_manual);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), show_manual);

  separator27 = gtk_menu_item_new ();
  gtk_widget_show (separator27);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), separator27);
  gtk_widget_set_sensitive (separator27, FALSE);

  donate = gtk_menu_item_new_with_mnemonic (_("_Donate to the project !"));
  gtk_widget_show (donate);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), donate);

  email_author = gtk_menu_item_new_with_mnemonic (_("_Email the author"));
  gtk_widget_show (email_author);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), email_author);

  report_bug = gtk_menu_item_new_with_mnemonic (_("Report a _bug"));
  gtk_widget_show (report_bug);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), report_bug);

  suggest_feature = gtk_menu_item_new_with_mnemonic (_("Suggest a _feature"));
  gtk_widget_show (suggest_feature);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), suggest_feature);

  help_translate = gtk_menu_item_new_with_mnemonic (_("Assist with _translating"));
  gtk_widget_show (help_translate);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), help_translate);

  separator27 = gtk_menu_item_new ();
  gtk_widget_show (separator27);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), separator27);
  gtk_widget_set_sensitive (separator27, FALSE);

  mainw->troubleshoot=gtk_menu_item_new_with_mnemonic (_("_Troubleshoot"));
  gtk_widget_show (mainw->troubleshoot);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), mainw->troubleshoot);

  about = gtk_menu_item_new_with_mnemonic (_("_About"));
  gtk_widget_show (about);
  gtk_container_add (GTK_CONTAINER (menuitem14_menu), about);

  mainw->btoolbar=gtk_toolbar_new();
  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(mainw->btoolbar),TRUE);
  gtk_box_pack_start (GTK_BOX (mainw->menu_hbox), mainw->btoolbar, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->btoolbar, GTK_STATE_NORMAL, &palette->menu_and_bars);
  }

  gtk_toolbar_set_style (GTK_TOOLBAR (mainw->btoolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR(mainw->btoolbar),GTK_ICON_SIZE_MENU);
  
  if (capable->smog_version_correct) {
    fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
    g_snprintf (buff,PATH_MAX,"%s",fnamex);
    g_free(fnamex);
    tmp_toolbar_icon=gtk_image_new_from_file (buff);
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
      gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }

    mainw->m_sepwinbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_sepwinbutton),0);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_sepwinbutton),_("Show the play window (s)"));
    
    tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-media-rewind", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
    
    mainw->m_rewindbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_rewindbutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_rewindbutton),_("Rewind to start (w)"));
    
    gtk_widget_set_sensitive(mainw->m_rewindbutton,FALSE);
    
    tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-media-play", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
    
    mainw->m_playbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_playbutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_playbutton),_("Play all (p)"));
    gtk_widget_modify_base (mainw->m_playbutton, GTK_STATE_PRELIGHT, &palette->menu_and_bars);
    
    gtk_widget_set_sensitive(mainw->m_playbutton,FALSE);


    tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-media-stop", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
    
    mainw->m_stopbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_stopbutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_stopbutton),_("Stop playback (q)"));
    
    gtk_widget_set_sensitive(mainw->m_stopbutton,FALSE);
    
    fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"playsel.png",NULL);
    g_snprintf (buff,PATH_MAX,"%s",fnamex);
    g_free(fnamex);
    tmp_toolbar_icon=gtk_image_new_from_file (buff);
    
    mainw->m_playselbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_playselbutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_playselbutton),_("Play selection (y)"));
    
    gtk_widget_set_sensitive(mainw->m_playselbutton,FALSE);


    fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
    g_snprintf (buff,PATH_MAX,"%s",fnamex);
    g_free(fnamex);
    tmp_toolbar_icon=gtk_image_new_from_file (buff);
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
      gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    
    mainw->m_loopbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_loopbutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_loopbutton),_("Switch continuous looping on (o)"));
    
    fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
    g_snprintf (buff,PATH_MAX,"%s",fnamex);
    g_free(fnamex);
    tmp_toolbar_icon=gtk_image_new_from_file (buff);
    if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
      pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
      gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }
    
    mainw->m_mutebutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->m_mutebutton),-1);
    gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->m_mutebutton),_("Mute the audio (z)"));

    gtk_widget_show_all(mainw->btoolbar);

  }
  else {
    mainw->m_sepwinbutton = gtk_menu_item_new ();
    mainw->m_rewindbutton = gtk_menu_item_new ();
    mainw->m_playbutton = gtk_menu_item_new ();
    mainw->m_stopbutton = gtk_menu_item_new ();
    mainw->m_playselbutton = gtk_menu_item_new ();
    mainw->m_loopbutton = gtk_menu_item_new ();
    mainw->m_mutebutton = gtk_menu_item_new ();
  }

  mainw->vol_label=GTK_WIDGET(gtk_tool_item_new());
  label=gtk_label_new(_("Volume"));
  gtk_container_add(GTK_CONTAINER(mainw->vol_label),label);

#if GTK_CHECK_VERSION(2,16,0) 
  mainw->volume_scale=gtk_volume_button_new();
  gtk_scale_button_set_value(GTK_SCALE_BUTTON(mainw->volume_scale),mainw->volume);
  gtk_scale_button_set_orientation (GTK_SCALE_BUTTON(mainw->volume_scale),GTK_ORIENTATION_HORIZONTAL);
#else
  mainw->volume_scale=gtk_hscale_new(GTK_ADJUSTMENT(spinbutton_adj));
  gtk_scale_set_draw_value(GTK_SCALE(mainw->volume_scale),FALSE);
  if (capable->smog_version_correct) {
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->vol_label),-1);
  }
#endif

  gtk_widget_show(mainw->volume_scale);
  gtk_widget_show(mainw->vol_label);

  mainw->vol_toolitem=GTK_WIDGET(gtk_tool_item_new());
  gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(mainw->vol_toolitem),FALSE);
  gtk_tool_item_set_expand(GTK_TOOL_ITEM(mainw->vol_toolitem),TRUE);

  if ((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio)) gtk_widget_show(mainw->vol_toolitem);

  gtk_container_add(GTK_CONTAINER(mainw->vol_toolitem),mainw->volume_scale);
  if (capable->smog_version_correct) {
    gtk_toolbar_insert(GTK_TOOLBAR(mainw->btoolbar),GTK_TOOL_ITEM(mainw->vol_toolitem),-1);
  }
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->vol_toolitem),_("Audio volume (1.00)"));

  g_signal_connect_after (GTK_OBJECT (mainw->volume_scale), "value_changed",
			  G_CALLBACK (on_volume_slider_value_changed),
			  NULL);

  mainw->play_window=NULL;

  mainw->tb_hbox=gtk_hbox_new (FALSE, 0);
  mainw->toolbar = gtk_toolbar_new ();
  gtk_widget_modify_bg (mainw->tb_hbox, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->toolbar, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_toolbar_set_show_arrow(GTK_TOOLBAR(mainw->toolbar),FALSE);

  gtk_box_pack_start (GTK_BOX (mainw->vbox1), mainw->tb_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mainw->tb_hbox), mainw->toolbar, TRUE, TRUE, 0);

  gtk_toolbar_set_style (GTK_TOOLBAR (mainw->toolbar), GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_icon_size (GTK_TOOLBAR(mainw->toolbar),GTK_ICON_SIZE_SMALL_TOOLBAR);
  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-stop", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));
  
  mainw->t_stopbutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_stopbutton),0);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_stopbutton),_("Stop playback (q)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-undo", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_bckground=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_bckground),1);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_bckground),_("Unblank background (b)"));

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  tmp_toolbar_icon=gtk_image_new_from_file (buff);
  if (g_file_test(buff,G_FILE_TEST_EXISTS)&&!mainw->sep_win) {
    pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
    gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->t_sepwin=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_sepwin),2);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_sepwin),_("Play in separate window (s)"));

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-in.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  tmp_toolbar_icon=gtk_image_new_from_file (buff);
  if (g_file_test(buff,G_FILE_TEST_EXISTS)&&!mainw->double_size) {
    pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
    gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }
  mainw->t_double=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_double),_("Double size (d)"));

  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_double),3);

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"fullscreen.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  tmp_toolbar_icon=gtk_image_new_from_file (buff);
  if (g_file_test(buff,G_FILE_TEST_EXISTS)) {
    pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(tmp_toolbar_icon));
    gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->t_fullscreen=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_fullscreen),4);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_fullscreen),_("Fullscreen playback (f)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-remove", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));


  mainw->t_slower=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_slower),5);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_slower),_("Play slower (ctrl-down)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-add", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_faster=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_faster),6);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_faster),_("Play faster (ctrl-up)"));


  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-go-back", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_back=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_back),7);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_back),_("Skip back (ctrl-left)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-go-forward", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_forward=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_forward),8);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_forward),_("Skip forward (ctrl-right)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-dialog-info", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_infobutton=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_infobutton),9);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_infobutton),_("Show clip info (i)"));

  tmp_toolbar_icon = gtk_image_new_from_stock ("gtk-cancel", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->toolbar)));

  mainw->t_hide=GTK_WIDGET(gtk_tool_button_new(GTK_WIDGET(tmp_toolbar_icon),""));
  gtk_toolbar_insert(GTK_TOOLBAR(mainw->toolbar),GTK_TOOL_ITEM(mainw->t_hide),10);
  gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mainw->t_hide),_("Hide this toolbar"));

  t_label=gtk_label_new(_ ("Press \"s\" to toggle separate play window for improved performance, \"q\" to stop."));
  gtk_widget_modify_fg(t_label, GTK_STATE_NORMAL, &palette->white);
  gtk_box_pack_start (GTK_BOX (mainw->tb_hbox), t_label, FALSE, FALSE, 0);

  gtk_widget_show_all (mainw->tb_hbox);
  gtk_widget_hide (mainw->tb_hbox);

  vbox4 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox4);

  mainw->eventbox = gtk_event_box_new ();
  gtk_widget_show (mainw->eventbox);
  gtk_box_pack_start (GTK_BOX (mainw->vbox1), mainw->eventbox, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (mainw->eventbox), vbox4);
  gtk_widget_modify_bg (mainw->eventbox, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_widget_set_events (mainw->eventbox, GDK_SCROLL_MASK);

  g_signal_connect (GTK_OBJECT (mainw->eventbox), "scroll_event",
                      G_CALLBACK (on_mouse_scroll),
                      NULL);

  mainw->framebar = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (mainw->framebar);
  gtk_box_pack_start (GTK_BOX (vbox4), mainw->framebar, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (mainw->framebar), 2);

  /* TRANSLATORS: please keep the translated string the same length */
  mainw->vps_label = gtk_label_new (_("     Video playback speed (frames per second)        "));
  tmp=g_strdup(gtk_label_get_text(GTK_LABEL(mainw->vps_label)));
  if (strlen(tmp)>55) memset(tmp+55,0,1);
  gtk_label_set_text(GTK_LABEL(mainw->vps_label),tmp);
  g_free(tmp);


  gtk_widget_show (mainw->vps_label);
  gtk_box_pack_start (GTK_BOX (mainw->framebar), mainw->vps_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->vps_label), GTK_JUSTIFY_LEFT);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->vps_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  spinbutton_pb_fps_adj = (GObject *)gtk_adjustment_new (1, -FPS_MAX, FPS_MAX, 0.1, 0.01, 0.);
  mainw->spinbutton_pb_fps = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_pb_fps_adj), (GdkModifierType)0, 3);
  gtk_widget_show (mainw->spinbutton_pb_fps);
  gtk_box_pack_start (GTK_BOX (mainw->framebar), mainw->spinbutton_pb_fps, FALSE, TRUE, 0);
  gtk_widget_set_tooltip_text( mainw->spinbutton_pb_fps,_ ("Vary the video speed"));

  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mainw->spinbutton_pb_fps), TRUE);


  if (palette->style==STYLE_PLAIN) {
    mainw->banner = gtk_label_new ("             = <  L i V E S > =                ");
  }
  else {
    mainw->banner = gtk_label_new ("                                                ");
  }
  gtk_widget_show (mainw->banner);

  gtk_box_pack_start (GTK_BOX (mainw->framebar), mainw->banner, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->banner), GTK_JUSTIFY_CENTER);

  mainw->framecounter = gtk_entry_new ();
  gtk_widget_show (mainw->framecounter);
  gtk_box_pack_start (GTK_BOX (mainw->framebar), mainw->framecounter, FALSE, TRUE, 0);
  gtk_entry_set_editable (GTK_ENTRY (mainw->framecounter), FALSE);
  gtk_entry_set_has_frame (GTK_ENTRY (mainw->framecounter), FALSE);
  gtk_entry_set_width_chars (GTK_ENTRY (mainw->framecounter), 18);

  lives_widget_set_can_focus (mainw->framecounter, FALSE);

  mainw->curf_label = gtk_label_new (_("                                                            "));
  gtk_widget_show (mainw->curf_label);
  gtk_box_pack_start (GTK_BOX (mainw->framebar), mainw->curf_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->curf_label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->curf_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_hide(mainw->framebar);

  hbox1 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show(hbox1);
  gtk_box_pack_start (GTK_BOX (vbox4), hbox1, FALSE, FALSE, 0);
  gtk_widget_modify_bg (hbox1, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->eventbox3 = gtk_event_box_new ();
  gtk_widget_show (mainw->eventbox3);
  gtk_box_pack_start (GTK_BOX (hbox1), mainw->eventbox3, TRUE, FALSE, 0);
  gtk_widget_modify_bg (mainw->eventbox3, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->frame1 = gtk_frame_new (NULL);
  gtk_widget_show (mainw->frame1);
  gtk_container_add (GTK_CONTAINER (mainw->eventbox3), mainw->frame1);
  gtk_widget_modify_bg (mainw->frame1, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_container_set_border_width (GTK_CONTAINER(mainw->eventbox3), 10);
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame1), GTK_SHADOW_IN);

  gtk_widget_show (mainw->image272);
  gtk_container_add (GTK_CONTAINER (mainw->frame1), mainw->image272);

  label15 = gtk_label_new (_("First Frame"));
  gtk_widget_show (label15);
  gtk_frame_set_label_widget (GTK_FRAME (mainw->frame1), label15);
  gtk_label_set_justify (GTK_LABEL (label15), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label15, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->playframe = gtk_frame_new (NULL);
  gtk_widget_hide (mainw->playframe);
  gtk_box_pack_start (GTK_BOX (hbox1), mainw->playframe, TRUE, FALSE, 0);
  gtk_widget_set_size_request (mainw->playframe, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);
  gtk_container_set_border_width (GTK_CONTAINER (mainw->playframe), 10);
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->playframe), GTK_SHADOW_IN);
  gtk_widget_modify_bg (mainw->playframe, GTK_STATE_NORMAL, &palette->normal_back);

  pf_label = gtk_label_new (_("Play"));
  gtk_widget_show (pf_label);
  gtk_frame_set_label_widget (GTK_FRAME (mainw->playframe), pf_label);
  gtk_label_set_justify (GTK_LABEL (pf_label), GTK_JUSTIFY_LEFT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(pf_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->pl_eventbox = gtk_event_box_new ();
  gtk_widget_modify_bg (mainw->pl_eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_container_add (GTK_CONTAINER (mainw->playframe), mainw->pl_eventbox);
  gtk_widget_show(mainw->pl_eventbox);

  mainw->playarea = gtk_hbox_new (FALSE,0);

  gtk_container_add (GTK_CONTAINER (mainw->pl_eventbox), mainw->playarea);

  gtk_widget_set_app_paintable(mainw->playarea,TRUE);

  mainw->eventbox4 = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (hbox1), mainw->eventbox4, TRUE, FALSE, 0);
  gtk_widget_modify_bg (mainw->eventbox4, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_show (mainw->eventbox4);

  mainw->frame2 = gtk_frame_new (NULL);
  gtk_widget_show (mainw->frame2);
  gtk_container_add (GTK_CONTAINER (mainw->eventbox4), mainw->frame2);
  gtk_widget_modify_bg (mainw->frame2, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_container_set_border_width (GTK_CONTAINER(mainw->eventbox4), 10);
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame2), GTK_SHADOW_IN);
  
  gtk_widget_show (mainw->image273);
  gtk_container_add (GTK_CONTAINER (mainw->frame2), mainw->image273);

  // default frame sizes
  mainw->def_width=DEFAULT_FRAME_HSIZE;
  mainw->def_height=DEFAULT_FRAME_HSIZE;

  if (!(mainw->imframe==NULL)) {
    if (lives_pixbuf_get_width (mainw->imframe)+H_RESIZE_ADJUST<mainw->def_width) {
      mainw->def_width=lives_pixbuf_get_width (mainw->imframe)+H_RESIZE_ADJUST;
    }
    if (lives_pixbuf_get_height (mainw->imframe)+V_RESIZE_ADJUST*mainw->foreign<mainw->def_height) {
      mainw->def_height=lives_pixbuf_get_height (mainw->imframe)+V_RESIZE_ADJUST*mainw->foreign;
    }
  }

  gtk_widget_set_size_request (mainw->eventbox3, mainw->def_width, mainw->def_height);
  gtk_widget_set_size_request (mainw->frame1, mainw->def_width, mainw->def_height);
  gtk_widget_set_size_request (mainw->frame2, mainw->def_width, mainw->def_height);
  gtk_widget_set_size_request (mainw->eventbox4, mainw->def_width, mainw->def_height);
  
  // the actual playback image for the internal player
  mainw->image274 = gtk_image_new_from_pixbuf (NULL);
  gtk_widget_show (mainw->image274);
  gtk_widget_ref(mainw->image274);
  gtk_object_sink (GTK_OBJECT (mainw->image274));

  label16 = gtk_label_new (_("Last Frame"));
  gtk_widget_show (label16);
  gtk_frame_set_label_widget (GTK_FRAME (mainw->frame2), label16);
  gtk_label_set_justify (GTK_LABEL (label16), GTK_JUSTIFY_RIGHT);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label16, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  hbox3 = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox3);
  gtk_box_pack_start (GTK_BOX (vbox4), hbox3, FALSE, TRUE, 0);

  spinbutton_start_adj = (GObject *)gtk_adjustment_new (0., 0., 0., 1., 100., 0.);
  mainw->spinbutton_start = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_start_adj), 1, 0);

  gtk_widget_show (mainw->spinbutton_start);
  gtk_box_pack_start (GTK_BOX (hbox3), mainw->spinbutton_start, TRUE, FALSE, 0);
  lives_widget_set_can_focus_and_default (mainw->spinbutton_start);

  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mainw->spinbutton_start), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (mainw->spinbutton_start),GTK_UPDATE_ALWAYS);
  gtk_widget_set_tooltip_text( mainw->spinbutton_start, _("The first selected frame in this clip"));

  mainw->arrow1 = gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_OUT);
  gtk_widget_show (mainw->arrow1);
  gtk_box_pack_start (GTK_BOX (hbox3), mainw->arrow1, FALSE, FALSE, 0);
  gtk_widget_modify_fg(mainw->arrow1, GTK_STATE_NORMAL, &palette->normal_fore);

  gtk_entry_set_width_chars (GTK_ENTRY (mainw->spinbutton_start),10);
  mainw->sel_label = gtk_label_new(NULL);

  set_sel_label(mainw->sel_label);
  gtk_widget_show (mainw->sel_label);
  gtk_box_pack_start (GTK_BOX (hbox3), mainw->sel_label, FALSE, FALSE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->sel_label), GTK_JUSTIFY_LEFT);
  gtk_widget_modify_fg(mainw->sel_label, GTK_STATE_NORMAL, &palette->normal_fore);

  mainw->arrow2 = gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
  gtk_widget_show (mainw->arrow2);
  gtk_box_pack_start (GTK_BOX (hbox3), mainw->arrow2, FALSE, FALSE, 0);
  gtk_widget_modify_fg(mainw->arrow2, GTK_STATE_NORMAL, &palette->normal_fore);

  spinbutton_end_adj = (GObject *)gtk_adjustment_new (0., 0., 0., 1., 100., 0.);
  mainw->spinbutton_end = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_end_adj), 1, 0);
  gtk_widget_show (mainw->spinbutton_end);
  gtk_box_pack_start (GTK_BOX (hbox3), mainw->spinbutton_end, TRUE, FALSE, 0);
  gtk_entry_set_width_chars (GTK_ENTRY (mainw->spinbutton_end),10);
  lives_widget_set_can_focus_and_default (mainw->spinbutton_end);

  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (mainw->spinbutton_end), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (mainw->spinbutton_end),GTK_UPDATE_ALWAYS);
  gtk_widget_set_tooltip_text( mainw->spinbutton_end, _("The last selected frame in this clip"));

  if (palette->style&STYLE_1&&palette->style&STYLE_2) {
    gtk_widget_modify_base(mainw->spinbutton_start, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_base(mainw->spinbutton_start, GTK_STATE_INSENSITIVE, &palette->normal_back);
    gtk_widget_modify_base(mainw->spinbutton_end, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_widget_modify_base(mainw->spinbutton_end, GTK_STATE_INSENSITIVE, &palette->normal_back);
    gtk_widget_modify_text(mainw->spinbutton_start, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_text(mainw->spinbutton_start, GTK_STATE_INSENSITIVE, &palette->normal_fore);
    gtk_widget_modify_text(mainw->spinbutton_end, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_text(mainw->spinbutton_end, GTK_STATE_INSENSITIVE, &palette->normal_fore);
    gtk_widget_modify_fg(mainw->sel_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_set_sensitive(mainw->spinbutton_start,FALSE);
  gtk_widget_set_sensitive(mainw->spinbutton_end,FALSE);

  mainw->hseparator = gtk_hseparator_new ();

  if (palette->style&STYLE_1) {
    gtk_box_pack_start (GTK_BOX (vbox4), mainw->sep_image, FALSE, TRUE, 20);
    gtk_widget_show (mainw->sep_image);
  }

  else {
    gtk_box_pack_start (GTK_BOX (vbox4), mainw->hseparator, TRUE, TRUE, 0);
    gtk_widget_show (mainw->hseparator);
  }

  mainw->eventbox5 = gtk_event_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox4), mainw->eventbox5, FALSE, FALSE, 0);

  gtk_widget_modify_bg (mainw->eventbox5, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->hruler = lives_standard_hruler_new();
  lives_ruler_set_range (LIVES_RULER (mainw->hruler), 0., 1000000., 0., 1000000.);

  gtk_widget_set_size_request (mainw->hruler, -1, 20);
  gtk_container_add (GTK_CONTAINER (mainw->eventbox5), mainw->hruler);

  gtk_widget_modify_bg (mainw->hruler, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_add_events (mainw->eventbox5, GDK_POINTER_MOTION_MASK | GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | 
			 GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg (mainw->hruler, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  mainw->eventbox2 = gtk_event_box_new ();
  gtk_widget_show (mainw->eventbox2);
  gtk_box_pack_start (GTK_BOX (vbox4), mainw->eventbox2, TRUE, TRUE, 0);
  gtk_widget_add_events (mainw->eventbox2, GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_modify_bg (mainw->eventbox2, GTK_STATE_NORMAL, &palette->normal_back);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (vbox2);
  gtk_container_add (GTK_CONTAINER (mainw->eventbox2), vbox2);

  mainw->vidbar = gtk_label_new (_("Video"));

  if (palette->style==STYLE_PLAIN) {
    gtk_widget_show (mainw->vidbar);
  }
  else {
    gtk_widget_modify_fg (mainw->vidbar, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_hide (mainw->vidbar);
  }

  gtk_label_set_justify (GTK_LABEL (mainw->vidbar), GTK_JUSTIFY_LEFT);
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->vidbar, TRUE, TRUE, 0);

  mainw->video_draw = gtk_drawing_area_new ();
  gtk_drawing_area_size(GTK_DRAWING_AREA(mainw->video_draw),lives_widget_get_allocation_width(mainw->LiVES),10);
  gtk_widget_show (mainw->video_draw);
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->video_draw, TRUE, TRUE, 0);
  gtk_widget_modify_bg (mainw->video_draw, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->laudbar = gtk_label_new (_("Left Audio"));
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->laudbar, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->laudbar), GTK_JUSTIFY_LEFT);

  if (palette->style==STYLE_PLAIN) {
    gtk_widget_show (mainw->laudbar);
  }
  else {
    gtk_widget_modify_fg (mainw->laudbar, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_hide (mainw->laudbar);
  }

  mainw->laudio_draw = gtk_drawing_area_new ();
  gtk_drawing_area_size(GTK_DRAWING_AREA(mainw->laudio_draw),lives_widget_get_allocation_width(mainw->LiVES),10);
  gtk_widget_show (mainw->laudio_draw);
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->laudio_draw, TRUE, TRUE, 0);
  gtk_widget_modify_bg (mainw->laudio_draw, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->raudbar = gtk_label_new (_("Right Audio"));
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->raudbar, TRUE, TRUE, 0);
  gtk_label_set_justify (GTK_LABEL (mainw->raudbar), GTK_JUSTIFY_LEFT);

  if (palette->style==STYLE_PLAIN) {
    gtk_widget_show (mainw->raudbar);
  }
  else {
    gtk_widget_modify_fg (mainw->raudbar, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_hide (mainw->raudbar);
  }

  mainw->raudio_draw = gtk_drawing_area_new ();
  gtk_drawing_area_size(GTK_DRAWING_AREA(mainw->raudio_draw),lives_widget_get_allocation_width(mainw->LiVES),10);
  gtk_widget_show (mainw->raudio_draw);
  gtk_box_pack_start (GTK_BOX (vbox2), mainw->raudio_draw, TRUE, TRUE, 0);
  gtk_widget_modify_bg (mainw->raudio_draw, GTK_STATE_NORMAL, &palette->normal_back);

  mainw->message_box=gtk_vbox_new(FALSE, 0);
  gtk_widget_show (mainw->message_box);
  gtk_box_pack_start (GTK_BOX (vbox4), mainw->message_box, TRUE, TRUE, 0);

  mainw->textview1=NULL;
  mainw->scrolledwindow=NULL;
  add_message_scroller(mainw->message_box);

  // accel keys
  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Page_Up, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (prevclip_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Page_Down, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (nextclip_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Down, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (slower_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Up, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (faster_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Left, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (skip_back_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Right, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (skip_forward_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_space, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (dirchange_callback),GINT_TO_POINTER(TRUE),NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_Return, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (fps_reset_callback),GINT_TO_POINTER(TRUE),NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_k, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (grabkeys_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_t, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (textparm_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_m, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (rtemode_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_x, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (swap_fg_bg_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_n, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (nervous_callback),NULL,NULL));

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_w, (GdkModifierType)0, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (show_sync_callback),NULL,NULL));

  if (FN_KEYS>0) {
    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F1, (GdkModifierType)0, (GtkAccelFlags)0, 
			     g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (1),NULL));
    if (FN_KEYS>1) {
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F2, (GdkModifierType)0, (GtkAccelFlags)0, 
			       g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (2),NULL));
      if (FN_KEYS>2) {
	gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F3, (GdkModifierType)0, (GtkAccelFlags)0, 
				 g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (3),NULL));
	if (FN_KEYS>3) {
	  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F4, (GdkModifierType)0, (GtkAccelFlags)0, 
				   g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (4),NULL));
	  if (FN_KEYS>4) {
	    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F5, (GdkModifierType)0, (GtkAccelFlags)0, 
				     g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (5),NULL));
	    if (FN_KEYS>5) {
	      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F6, (GdkModifierType)0, (GtkAccelFlags)0, 
				       g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (6),NULL));
	      if (FN_KEYS>6) {
		gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F7, (GdkModifierType)0, (GtkAccelFlags)0, 
					 g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (7),NULL));
		if (FN_KEYS>7) {
		  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F8, (GdkModifierType)0, (GtkAccelFlags)0, 
					   g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (8),NULL));
		  if (FN_KEYS>8) {
		    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F9, (GdkModifierType)0, (GtkAccelFlags)0, 
					     g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (9),NULL));
		    if (FN_KEYS>9) {
		      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F10, (GdkModifierType)0, (GtkAccelFlags)0, 
					       g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (10),NULL));
		      if (FN_KEYS>10) {
		      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F11, (GdkModifierType)0, (GtkAccelFlags)0, 
					       g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (11),NULL));
		      if (FN_KEYS>11) {
		      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_F12, (GdkModifierType)0, (GtkAccelFlags)0, 
					       g_cclosure_new (G_CALLBACK (storeclip_callback),GINT_TO_POINTER (12),NULL));
		      // ad nauseum...
		      }}}}}}}}}}}}

  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_0, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			   g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (0),NULL));
  if (FX_KEYS_PHYSICAL>0) {
    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_1, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			     g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (1),NULL));
    if (FX_KEYS_PHYSICAL>1) {
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_2, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
			       g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (2),NULL));
      if (FX_KEYS_PHYSICAL>2) {
	gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_3, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
				 g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (3),NULL));
	if (FX_KEYS_PHYSICAL>3) {
	  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_4, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
				   g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (4),NULL));
	  if (FX_KEYS_PHYSICAL>4) {
	    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_5, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
				     g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (5),NULL));
	    if (FX_KEYS_PHYSICAL>5) {
	      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_6, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
				       g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (6),NULL));
	      if (FX_KEYS_PHYSICAL>6) {
		gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_7, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
					 g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (7),NULL));
		if (FX_KEYS_PHYSICAL>7) {
		  gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_8, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
					   g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (8),NULL));
		  if (FX_KEYS_PHYSICAL>8) {
		    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_9, GDK_CONTROL_MASK, (GtkAccelFlags)0, 
					     g_cclosure_new (G_CALLBACK (rte_on_off_callback),GINT_TO_POINTER (9),NULL));
		  }}}}}}}}}




  gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (mainw->textview1)),
	_("Starting...\n"), -1);
  
  g_signal_connect (GTK_OBJECT (mainw->LiVES), "delete_event",
		    G_CALLBACK (on_LiVES_delete_event),
		    NULL);

  mainw->config_func=g_signal_connect_after (GTK_OBJECT (mainw->video_draw), "configure_event",
					     G_CALLBACK (config_event),
					     NULL);
  mainw->vidbar_func=g_signal_connect_after (GTK_OBJECT (mainw->video_draw), "expose_event",
		    G_CALLBACK (expose_vid_event),
		    NULL);
  mainw->laudbar_func=g_signal_connect_after (GTK_OBJECT (mainw->laudio_draw), "expose_event",
		    G_CALLBACK (expose_laud_event),
		    NULL);
  mainw->raudbar_func=g_signal_connect_after (GTK_OBJECT (mainw->raudio_draw), "expose_event",
		    G_CALLBACK (expose_raud_event),
		    NULL);
  mainw->pb_fps_func=g_signal_connect_after (GTK_OBJECT (mainw->spinbutton_pb_fps), "value_changed",
                      G_CALLBACK (changed_fps_during_pb),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->open), "activate",
		    G_CALLBACK (on_open_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->open_sel), "activate",
		    G_CALLBACK (on_open_sel_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->open_dvd), "activate",
		    G_CALLBACK (on_open_vcd_activate),
		    GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (mainw->open_vcd), "activate",
		    G_CALLBACK (on_open_vcd_activate),
		    GINT_TO_POINTER (2));
  g_signal_connect (GTK_OBJECT (mainw->open_loc), "activate",
		    G_CALLBACK (on_open_loc_activate),
		    NULL);
#ifdef HAVE_WEBM
  g_signal_connect (GTK_OBJECT (mainw->open_utube), "activate",
		    G_CALLBACK (on_open_utube_activate),
		    NULL);
#endif

#ifdef HAVE_LDVGRAB
  g_signal_connect (GTK_OBJECT (mainw->open_firewire), "activate",
		    G_CALLBACK (on_open_fw_activate),
		    GINT_TO_POINTER(CAM_FORMAT_DV));
  g_signal_connect (GTK_OBJECT (mainw->open_hfirewire), "activate",
		    G_CALLBACK (on_open_fw_activate),
		    GINT_TO_POINTER(CAM_FORMAT_HDV));
#endif

#ifdef HAVE_YUV4MPEG
  if (capable->smog_version_correct) {
    g_signal_connect (GTK_OBJECT (mainw->open_yuv4m), "activate",
		      G_CALLBACK (on_open_yuv4m_activate),
		      NULL);
  }
#endif
  g_signal_connect (GTK_OBJECT (mainw->open_lives2lives), "activate",
		    G_CALLBACK (on_open_lives2lives_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->send_lives2lives), "activate",
		    G_CALLBACK (on_send_lives2lives_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->recent1), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(1));
  g_signal_connect (GTK_OBJECT (mainw->recent2), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(2));
  g_signal_connect (GTK_OBJECT (mainw->recent3), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(3));
  g_signal_connect (GTK_OBJECT (mainw->recent4), "activate",
                      G_CALLBACK (on_recent_activate),
		    GINT_TO_POINTER(4));
  g_signal_connect (GTK_OBJECT (mainw->backup), "activate",
                      G_CALLBACK (on_backup_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->restore), "activate",
                      G_CALLBACK (on_restore_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->save_as), "activate",
		    G_CALLBACK (on_save_as_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->save_selection), "activate",
		    G_CALLBACK (on_save_selection_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->close), "activate",
                      G_CALLBACK (on_close_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->import_proj), "activate",
                      G_CALLBACK (on_import_proj_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->export_proj), "activate",
                      G_CALLBACK (on_export_proj_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->clear_ds), "activate",
                      G_CALLBACK (on_cleardisk_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->quit), "activate",
                      G_CALLBACK (on_quit_activate),
                      GINT_TO_POINTER(0));
  g_signal_connect (GTK_OBJECT (mainw->vj_save_set), "activate",
                      G_CALLBACK (on_quit_activate),
                      GINT_TO_POINTER(1));
  g_signal_connect (GTK_OBJECT (mainw->undo), "activate",
                      G_CALLBACK (on_undo_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->redo), "activate",
                      G_CALLBACK (on_redo_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->copy), "activate",
                      G_CALLBACK (on_copy_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->mt_menu), "activate",
                      G_CALLBACK (on_multitrack_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->cut), "activate",
                      G_CALLBACK (on_cut_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->insert), "activate",
                      G_CALLBACK (on_insert_pre_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->merge), "activate",
                      G_CALLBACK (on_merge_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->paste_as_new), "activate",
                      G_CALLBACK (on_paste_as_new_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->xdelete), "activate",
                      G_CALLBACK (on_delete_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_all), "activate",
                      G_CALLBACK (on_select_all_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_start_only), "activate",
                      G_CALLBACK (on_select_start_only_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_end_only), "activate",
                      G_CALLBACK (on_select_end_only_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_invert), "activate",
                      G_CALLBACK (on_select_invert_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_new), "activate",
                      G_CALLBACK (on_select_new_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_to_end), "activate",
                      G_CALLBACK (on_select_to_end_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_from_start), "activate",
                      G_CALLBACK (on_select_from_start_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->select_last), "activate",
                      G_CALLBACK (on_select_last_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->lock_selwidth), "activate",
                      G_CALLBACK (on_lock_selwidth_activate),
                      NULL);
  mainw->record_perf_func=g_signal_connect (GTK_OBJECT (mainw->record_perf), "activate",
                      G_CALLBACK (on_record_perf_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->playall), "activate",
                      G_CALLBACK (on_playall_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->rewind), "activate",
                      G_CALLBACK (on_rewind_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->playsel), "activate",
                      G_CALLBACK (on_playsel_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->playclip), "activate",
                      G_CALLBACK (on_playclip_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->stop), "activate",
                      G_CALLBACK (on_stop_activate),
                      NULL);
  mainw->fullscreen_cb_func=g_signal_connect (GTK_OBJECT (mainw->full_screen), "activate",
					      G_CALLBACK (on_full_screen_activate),
					      NULL);
  g_signal_connect (GTK_OBJECT (mainw->sw_sound), "activate",
                      G_CALLBACK (on_boolean_toggled),
		    &mainw->save_with_sound); // TODO - make pref
  g_signal_connect (GTK_OBJECT (mainw->showsubs), "activate",
                      G_CALLBACK (on_showsubs_toggled),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->letter), "activate",
                      G_CALLBACK (on_boolean_toggled),
                      &prefs->letterbox);
  g_signal_connect (GTK_OBJECT (mainw->aload_subs), "activate",
                      G_CALLBACK (on_boolean_toggled),
                      &prefs->autoload_subs);
  g_signal_connect (GTK_OBJECT (mainw->ccpd_sound), "activate",
                      G_CALLBACK (on_boolean_toggled),
		    &mainw->ccpd_with_sound); // TODO - make pref
  g_signal_connect (GTK_OBJECT (mainw->dsize), "activate",
                      G_CALLBACK (on_double_size_activate),
                      NULL);
  mainw->sepwin_cb_func=g_signal_connect (GTK_OBJECT (mainw->sepwin), "activate",
					  G_CALLBACK (on_sepwin_activate),
					  NULL);
  g_signal_connect (GTK_OBJECT (mainw->fade), "activate",
                      G_CALLBACK (on_fade_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->loop_video), "activate",
                      G_CALLBACK (on_loop_video_activate),
                      NULL);
  mainw->loop_cont_func=g_signal_connect (GTK_OBJECT (mainw->loop_continue), "activate",
					  G_CALLBACK (on_loop_cont_activate),
					  NULL);
  g_signal_connect (GTK_OBJECT (mainw->loop_ping_pong), "activate",
                      G_CALLBACK (on_ping_pong_activate),
                      NULL);
  mainw->mute_audio_func=g_signal_connect (GTK_OBJECT (mainw->mute_audio), "activate",
					   G_CALLBACK (on_mute_activate),
					   NULL);
  g_signal_connect (GTK_OBJECT (mainw->sticky), "activate",
                      G_CALLBACK (on_sticky_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->showfct), "activate",
                      G_CALLBACK (on_showfct_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->preferences), "activate",
                      G_CALLBACK (on_preferences_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->change_speed), "activate",
                      G_CALLBACK (on_change_speed_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->resample_video), "activate",
                      G_CALLBACK (on_resample_video_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->load_subs), "activate",
                      G_CALLBACK (on_load_subs_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->erase_subs), "activate",
                      G_CALLBACK (on_erase_subs_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->capture), "activate",
                      G_CALLBACK (on_capture_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->rev_clipboard), "activate",
                      G_CALLBACK (on_rev_clipboard_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->export_selaudio), "activate",
                      G_CALLBACK (on_export_audio_activate),
                      GINT_TO_POINTER (0));
  g_signal_connect (GTK_OBJECT (mainw->export_allaudio), "activate",
                      G_CALLBACK (on_export_audio_activate),
                      GINT_TO_POINTER(1));
  g_signal_connect (GTK_OBJECT (mainw->append_audio), "activate",
                      G_CALLBACK (on_append_audio_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->trim_audio), "activate",
                      G_CALLBACK (on_trim_audio_activate),
                      GINT_TO_POINTER (0));
  g_signal_connect (GTK_OBJECT (mainw->trim_to_pstart), "activate",
                      G_CALLBACK (on_trim_audio_activate),
                      GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (mainw->delsel_audio), "activate",
                      G_CALLBACK (on_del_audio_activate),
                      GINT_TO_POINTER (0));
  g_signal_connect (GTK_OBJECT (mainw->fade_aud_in), "activate",
                      G_CALLBACK (on_fade_audio_activate),
                      GINT_TO_POINTER (0));
  g_signal_connect (GTK_OBJECT (mainw->fade_aud_out), "activate",
                      G_CALLBACK (on_fade_audio_activate),
                      GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (mainw->delall_audio), "activate",
                      G_CALLBACK (on_del_audio_activate),
                      GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (mainw->ins_silence), "activate",
                      G_CALLBACK (on_ins_silence_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->recaudio_clip), "activate",
                      G_CALLBACK (on_recaudclip_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->recaudio_sel), "activate",
                      G_CALLBACK (on_recaudsel_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->resample_audio), "activate",
                      G_CALLBACK (on_resample_audio_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->load_audio), "activate",
                      G_CALLBACK (on_load_audio_activate),
                      NULL);
  if (capable->has_cdda2wav) {
    g_signal_connect (GTK_OBJECT (mainw->load_cdtrack), "activate",
                      G_CALLBACK (on_load_cdtrack_activate),
                      NULL);
    
    g_signal_connect (GTK_OBJECT (mainw->eject_cd), "activate",
                      G_CALLBACK (on_eject_cd_activate),
                      NULL);
  }

  if (capable->has_xmms) {
    g_signal_connect (GTK_OBJECT (mainw->xmms_play_audio), "activate",
                      G_CALLBACK (on_xmms_play_audio_activate),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->xmms_random_audio), "activate",
                      G_CALLBACK (on_xmms_random_audio_activate),
                      NULL); 
    g_signal_connect (GTK_OBJECT (mainw->xmms_stop_audio), "activate",
                      G_CALLBACK (on_xmms_stop_audio_activate),
                      NULL);
  }

  g_signal_connect (GTK_OBJECT (mainw->show_file_info), "activate",
		    G_CALLBACK (on_show_file_info_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->show_file_comments), "activate",
		    G_CALLBACK (on_show_file_comments_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->show_clipboard_info), "activate",
		    G_CALLBACK (on_show_clipboard_info_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->show_messages), "activate",
		    G_CALLBACK (on_show_messages_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->show_layout_errors), "activate",
		    G_CALLBACK (popup_lmap_errors),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->rename), "activate",
		    G_CALLBACK (on_rename_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (new_test_rfx), "activate",
		    G_CALLBACK (on_new_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));
  g_signal_connect (GTK_OBJECT (copy_rfx), "activate",
		    G_CALLBACK (on_copy_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));
  g_signal_connect (GTK_OBJECT (mainw->edit_test_rfx), "activate",
		    G_CALLBACK (on_edit_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));
  g_signal_connect (GTK_OBJECT (mainw->rename_test_rfx), "activate",
		    G_CALLBACK (on_rename_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));
  g_signal_connect (GTK_OBJECT (rebuild_rfx), "activate",
		    G_CALLBACK (on_rebuild_rfx_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->delete_test_rfx), "activate",
		    G_CALLBACK (on_delete_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));
  g_signal_connect (GTK_OBJECT (mainw->delete_custom_rfx), "activate",
		    G_CALLBACK (on_delete_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_CUSTOM));
  g_signal_connect (GTK_OBJECT (import_custom_rfx), "activate",
		    G_CALLBACK (on_import_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_CUSTOM));
  g_signal_connect (GTK_OBJECT (mainw->export_custom_rfx), "activate",
		    G_CALLBACK (on_export_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_CUSTOM));
  g_signal_connect (GTK_OBJECT (mainw->promote_test_rfx), "activate",
		    G_CALLBACK (on_promote_rfx_activate),
		    GINT_TO_POINTER (RFX_STATUS_TEST));

  g_signal_connect (GTK_OBJECT (mainw->vj_load_set), "activate",
		    G_CALLBACK (on_load_set_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->vj_show_keys), "activate",
		    G_CALLBACK (on_show_keys_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (assign_rte_keys), "activate",
		    G_CALLBACK (on_assign_rte_keys_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->save_rte_defs), "activate",
		    G_CALLBACK (on_save_rte_defs_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (mainw->vj_reset), "activate",
		    G_CALLBACK (on_vj_reset_activate),
		    NULL);
#ifdef ENABLE_OSC
  g_signal_connect (GTK_OBJECT (midi_learn), "activate",
		    G_CALLBACK (on_midi_learn_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (midi_save), "activate",
		    G_CALLBACK (on_midi_save_activate),
		    NULL);
  g_signal_connect (GTK_OBJECT (midi_load), "activate",
		    G_CALLBACK (on_midi_load_activate),
		    NULL);
#endif

  mainw->toy_func_none=g_signal_connect_after (GTK_OBJECT (mainw->toy_none), "activate",
					 G_CALLBACK (on_toy_activate),
					 NULL);

  mainw->toy_func_autolives=g_signal_connect_after (GTK_OBJECT (mainw->toy_autolives), "activate",
						  G_CALLBACK (on_toy_activate),
						  GINT_TO_POINTER(LIVES_TOY_AUTOLIVES));

  mainw->toy_func_random_frames=g_signal_connect_after (GTK_OBJECT (mainw->toy_random_frames), "activate",
						  G_CALLBACK (on_toy_activate),
						  GINT_TO_POINTER(LIVES_TOY_MAD_FRAMES));

  mainw->toy_func_lives_tv=g_signal_connect_after (GTK_OBJECT (mainw->toy_tv), "activate",
					     G_CALLBACK (on_toy_activate),
					     GINT_TO_POINTER(LIVES_TOY_TV));

  g_signal_connect (GTK_OBJECT (about), "activate",
		    G_CALLBACK (on_about_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (mainw->troubleshoot), "activate",
		    G_CALLBACK (on_troubleshoot_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (show_manual), "activate",
		    G_CALLBACK (show_manual_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (email_author), "activate",
		    G_CALLBACK (email_author_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (donate), "activate",
		    G_CALLBACK (donate_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (report_bug), "activate",
		    G_CALLBACK (report_bug_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (suggest_feature), "activate",
		    G_CALLBACK (suggest_feature_activate),
		    NULL);

  g_signal_connect (GTK_OBJECT (help_translate), "activate",
		    G_CALLBACK (help_translate_activate),
		    NULL);

  mainw->spin_start_func=g_signal_connect_after (GTK_OBJECT (mainw->spinbutton_start), "value_changed",
						 G_CALLBACK (on_spinbutton_start_value_changed),
						 NULL);
  mainw->spin_end_func=g_signal_connect_after (GTK_OBJECT (mainw->spinbutton_end), "value_changed",
					       G_CALLBACK (on_spinbutton_end_value_changed),
					       NULL);

  // these are for the menu transport buttons
  if (capable->smog_version_correct) {
    g_signal_connect (GTK_OBJECT (mainw->m_sepwinbutton), "clicked",
		      G_CALLBACK (on_sepwin_pressed),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_playbutton), "clicked",
		      G_CALLBACK (on_playall_activate),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_stopbutton), "clicked",
		      G_CALLBACK (on_stop_activate),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_playselbutton), "clicked",
		      G_CALLBACK (on_playsel_activate),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_rewindbutton), "clicked",
		      G_CALLBACK (on_rewind_activate),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_mutebutton), "clicked",
		      G_CALLBACK (on_mute_button_activate),
		      NULL);
    g_signal_connect (GTK_OBJECT (mainw->m_loopbutton), "clicked",
		      G_CALLBACK (on_loop_button_activate),
		      NULL);
    
    // these are 'invisible' buttons for the key accelerators
    g_signal_connect (GTK_OBJECT (mainw->t_stopbutton), "clicked",
                      G_CALLBACK (on_stop_activate),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_bckground), "clicked",
                      G_CALLBACK (on_fade_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_sepwin), "clicked",
                      G_CALLBACK (on_sepwin_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_double), "clicked",
                      G_CALLBACK (on_double_size_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_fullscreen), "clicked",
                      G_CALLBACK (on_full_screen_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_infobutton), "clicked",
                      G_CALLBACK (on_show_file_info_activate),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_hide), "clicked",
                      G_CALLBACK (on_toolbar_hide),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_slower), "clicked",
                      G_CALLBACK (on_slower_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_faster), "clicked",
                      G_CALLBACK (on_faster_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_back), "clicked",
                      G_CALLBACK (on_back_pressed),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->t_forward), "clicked",
                      G_CALLBACK (on_forward_pressed),
                      NULL);
    
    mainw->mouse_fn1=g_signal_connect (GTK_OBJECT (mainw->eventbox2), "motion_notify_event",
				       G_CALLBACK (on_mouse_sel_update),
				       NULL);
    g_signal_handler_block (mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=TRUE;
    g_signal_connect (GTK_OBJECT (mainw->eventbox2), "button_release_event",
                      G_CALLBACK (on_mouse_sel_reset),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->eventbox2), "button_press_event",
                      G_CALLBACK (on_mouse_sel_start),
                      NULL);
    g_signal_connect (GTK_OBJECT (mainw->hruler), "motion_notify_event",
                      G_CALLBACK (return_true),
                      NULL);
    mainw->hrule_func=g_signal_connect (GTK_OBJECT (mainw->eventbox5), "motion_notify_event",
					G_CALLBACK (on_hrule_update),
					NULL);
    g_signal_handler_block (mainw->eventbox5,mainw->hrule_func);
    g_signal_connect (GTK_OBJECT(mainw->eventbox5), "enter-notify-event",G_CALLBACK (on_hrule_enter),NULL);
  }

  mainw->hrule_blocked=TRUE;
  g_signal_connect (GTK_OBJECT (mainw->eventbox5), "button_release_event",
                      G_CALLBACK (on_hrule_reset),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->eventbox5), "button_press_event",
                      G_CALLBACK (on_hrule_set),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->eventbox3), "button_press_event",
                      G_CALLBACK (frame_context),
                      GINT_TO_POINTER (1));
  g_signal_connect (GTK_OBJECT (mainw->eventbox4), "button_press_event",
                      G_CALLBACK (frame_context),
                      GINT_TO_POINTER (2));

  gtk_window_add_accel_group (GTK_WINDOW (mainw->LiVES), mainw->accel_group);
  mainw->laudio_drawable=NULL;
  mainw->raudio_drawable=NULL;
  mainw->video_drawable=NULL;
  mainw->plug=NULL;

  lives_widget_set_can_focus (mainw->LiVES, TRUE);
  gtk_widget_grab_focus(mainw->textview1);

}



void
fade_background(void) {

  if (palette->style==STYLE_PLAIN) {
    gtk_label_set_text(GTK_LABEL(mainw->banner),"            = <  L i V E S > =              ");
  }
  if (mainw->foreign) {
    gtk_label_set_text(GTK_LABEL(mainw->banner),_("    Press 'q' to stop recording.  DO NOT COVER THE PLAY WINDOW !   "));
    gtk_widget_modify_fg(mainw->banner, GTK_STATE_NORMAL, &palette->banner_fade_text);
    gtk_label_set_text(GTK_LABEL(mainw->vps_label),("                      "));
  }
  else {
    gtk_widget_modify_bg (mainw->playframe, GTK_STATE_NORMAL, &palette->fade_colour);
    if (mainw->sep_win) {
      gtk_widget_hide (mainw->playframe);
    }
    gtk_frame_set_shadow_type (GTK_FRAME(mainw->playframe), GTK_SHADOW_NONE);
  }

  gtk_frame_set_label(GTK_FRAME(mainw->playframe), "");

  gtk_widget_modify_fg (mainw->curf_label, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_fg (mainw->vps_label, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->vbox1, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->LiVES, GTK_STATE_NORMAL, &palette->fade_colour);

  gtk_widget_modify_bg (mainw->eventbox3, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->eventbox4, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->eventbox, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->pl_eventbox, GTK_STATE_NORMAL, &palette->fade_colour);

  gtk_widget_modify_bg (mainw->frame1, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_widget_modify_bg (mainw->frame2, GTK_STATE_NORMAL, &palette->fade_colour);
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame1), GTK_SHADOW_NONE);
  gtk_frame_set_label (GTK_FRAME(mainw->frame1), "");
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame2), GTK_SHADOW_NONE);
  gtk_frame_set_label (GTK_FRAME(mainw->frame2), "");

  if (mainw->toy_type!=LIVES_TOY_MAD_FRAMES||mainw->foreign) {
    gtk_widget_hide(mainw->image272);
    gtk_widget_hide(mainw->image273);
  }
  if (!mainw->foreign&&future_prefs->show_tool) {
    gtk_widget_show(mainw->tb_hbox);
  }

  gtk_widget_hide(mainw->menu_hbox);
  gtk_widget_hide(mainw->hseparator);
  gtk_widget_hide(mainw->sep_image);
  gtk_widget_hide(mainw->scrolledwindow);
  gtk_widget_hide(mainw->eventbox2);
  gtk_widget_hide(mainw->spinbutton_start);
  gtk_widget_hide(mainw->hruler);
  gtk_widget_hide(mainw->eventbox5);

  if (!mainw->foreign) {
    gtk_widget_show(mainw->t_forward);
    gtk_widget_show(mainw->t_back);
    gtk_widget_show(mainw->t_slower);
    gtk_widget_show(mainw->t_faster);
    gtk_widget_show(mainw->t_fullscreen);
    gtk_widget_show(mainw->t_sepwin);
  }

  gtk_widget_hide(mainw->spinbutton_start);
  gtk_widget_hide(mainw->spinbutton_end);
  gtk_widget_hide(mainw->sel_label);
  gtk_widget_hide(mainw->arrow1);
  gtk_widget_hide(mainw->arrow2);

  // since the hidden menu buttons are not activable on some window managers
  // we need to remove the accelerators and add accelerator keys instead

  if (stop_closure==NULL) {
    gtk_widget_remove_accelerator (mainw->stop, mainw->accel_group, GDK_q, (GdkModifierType)0);
    gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_q, (GdkModifierType)0, (GtkAccelFlags)0, 
			     (stop_closure=g_cclosure_new (G_CALLBACK (stop_callback),NULL,NULL)));

    if (!mainw->foreign) {
      // TODO - do these checks in the end functions
      gtk_widget_remove_accelerator (mainw->record_perf, mainw->accel_group, GDK_r, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_r, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (rec_closure=g_cclosure_new (G_CALLBACK (rec_callback),NULL,NULL)));
      
      gtk_widget_remove_accelerator (mainw->full_screen, mainw->accel_group, GDK_f, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_f, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (fullscreen_closure=g_cclosure_new (G_CALLBACK (fullscreen_callback),NULL,NULL)));
      
      gtk_widget_remove_accelerator (mainw->showfct, mainw->accel_group, GDK_h, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_h, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (showfct_closure=g_cclosure_new (G_CALLBACK (showfct_callback),NULL,NULL)));
      
      gtk_widget_remove_accelerator (mainw->showsubs, mainw->accel_group, GDK_v, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_v, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (showsubs_closure=g_cclosure_new (G_CALLBACK (showsubs_callback),NULL,NULL)));
      
      gtk_widget_remove_accelerator (mainw->sepwin, mainw->accel_group, GDK_s, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_s, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (sepwin_closure=g_cclosure_new (G_CALLBACK (sepwin_callback),NULL,NULL)));

      if (!cfile->achans||mainw->mute||mainw->loop_cont||prefs->audio_player==AUD_PLAYER_JACK||
	  prefs->audio_player==AUD_PLAYER_PULSE) {
	gtk_widget_remove_accelerator (mainw->loop_video, mainw->accel_group, GDK_l, (GdkModifierType)0);
	gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_l, (GdkModifierType)0, (GtkAccelFlags)0, 
				 (loop_closure=g_cclosure_new (G_CALLBACK (loop_callback),NULL,NULL)));
	
	gtk_widget_remove_accelerator (mainw->loop_continue, mainw->accel_group, GDK_o, (GdkModifierType)0);
	gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_o, (GdkModifierType)0, (GtkAccelFlags)0, 
				 (loop_cont_closure=g_cclosure_new (G_CALLBACK (loop_cont_callback),NULL,NULL)));
	
      }
      gtk_widget_remove_accelerator (mainw->loop_ping_pong, mainw->accel_group, GDK_g, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_g, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (ping_pong_closure=g_cclosure_new (G_CALLBACK (ping_pong_callback),NULL,NULL)));
      gtk_widget_remove_accelerator (mainw->mute_audio, mainw->accel_group, GDK_z, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_z, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (mute_audio_closure=g_cclosure_new (G_CALLBACK (mute_audio_callback),NULL,NULL)));
      gtk_widget_remove_accelerator (mainw->dsize, mainw->accel_group, GDK_d, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_d, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (dblsize_closure=g_cclosure_new (G_CALLBACK (dblsize_callback),NULL,NULL)));
      gtk_widget_remove_accelerator (mainw->fade, mainw->accel_group, GDK_b, (GdkModifierType)0);
      gtk_accel_group_connect (GTK_ACCEL_GROUP (mainw->accel_group), GDK_b, (GdkModifierType)0, (GtkAccelFlags)0, 
			       (fade_closure=g_cclosure_new (G_CALLBACK (fade_callback),NULL,NULL)));
    }
  }
}






void
unfade_background(void) {
  if (prefs->open_maximised&&prefs->show_gui) {
    gtk_window_maximize (GTK_WINDOW(mainw->LiVES));
  }

  if (palette->style==STYLE_PLAIN) {
    gtk_label_set_text(GTK_LABEL(mainw->banner),"   = <  L i V E S > =                            ");
  }
  else {
    gtk_label_set_text(GTK_LABEL(mainw->banner),"                                                ");
  }
  gtk_widget_modify_fg(mainw->banner, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_bg (mainw->eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_bg (mainw->vbox1, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_bg (mainw->eventbox3, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_frame_set_label (GTK_FRAME(mainw->frame1), _("First Frame"));
  if (!mainw->preview) {
    gtk_frame_set_label(GTK_FRAME(mainw->playframe),_("Play"));
  }
  else {
    gtk_frame_set_label(GTK_FRAME(mainw->playframe),_("Preview"));
  }

  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame1), GTK_SHADOW_IN);
  gtk_frame_set_label (GTK_FRAME(mainw->frame2), _("Last Frame"));
  gtk_widget_modify_fg (mainw->curf_label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_fg (mainw->vps_label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_bg (mainw->LiVES, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_widget_modify_bg (mainw->pl_eventbox, GTK_STATE_NORMAL, &palette->normal_back);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(gtk_frame_get_label_widget(GTK_FRAME(mainw->playframe)), GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(gtk_frame_get_label_widget(GTK_FRAME(mainw->frame1)), GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(gtk_frame_get_label_widget(GTK_FRAME(mainw->frame2)), GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_modify_bg (mainw->eventbox4, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_bg (mainw->frame1, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_widget_modify_bg (mainw->frame2, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_frame_set_shadow_type (GTK_FRAME(mainw->frame2), GTK_SHADOW_IN);
  gtk_widget_modify_bg (mainw->playframe, GTK_STATE_NORMAL, &palette->normal_back);
  gtk_frame_set_shadow_type (GTK_FRAME(mainw->playframe), GTK_SHADOW_IN);
  gtk_widget_show(mainw->menu_hbox);
  gtk_widget_hide(mainw->tb_hbox);
  gtk_widget_show(mainw->hseparator);
  if (!mainw->double_size||mainw->sep_win) {
    if (palette->style&STYLE_1) {
      gtk_widget_show(mainw->sep_image);
    }
    if (mainw->multitrack==NULL) gtk_widget_show(mainw->scrolledwindow);
  }
  gtk_widget_show(mainw->image272);
  gtk_widget_show(mainw->image273);
  gtk_widget_show(mainw->eventbox2);
  if (!cfile->opening) {
    gtk_widget_show(mainw->hruler);
    gtk_widget_show(mainw->eventbox5);
  }
  if (cfile->frames>0&&!(prefs->sepwin_type==1&&mainw->sep_win)) {
    gtk_widget_show(mainw->playframe);
  }
  gtk_widget_show(mainw->spinbutton_start);
  gtk_widget_show(mainw->spinbutton_end);
  gtk_widget_show(mainw->sel_label);
  gtk_widget_show(mainw->arrow1);
  gtk_widget_show(mainw->arrow2);
  gtk_widget_show(mainw->spinbutton_pb_fps);
  gtk_widget_modify_fg (mainw->vps_label, GTK_STATE_NORMAL, &palette->normal_fore);
  gtk_widget_modify_fg (mainw->curf_label, GTK_STATE_NORMAL, &palette->normal_fore);

  if (stop_closure!=NULL) {
    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), stop_closure);
    gtk_widget_add_accelerator (mainw->stop, "activate", mainw->accel_group,
				GDK_q, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);
    
    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), rec_closure);
    gtk_widget_add_accelerator (mainw->record_perf, "activate", mainw->accel_group,
				GDK_r, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);
    
    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), fullscreen_closure);
    gtk_widget_add_accelerator (mainw->full_screen, "activate", mainw->accel_group,
				  GDK_f, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);
    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), sepwin_closure);
    gtk_widget_add_accelerator (mainw->sepwin, "activate", mainw->accel_group,
				GDK_s, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), mute_audio_closure);
    gtk_widget_add_accelerator (mainw->mute_audio, "activate", mainw->accel_group,
				GDK_z, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), ping_pong_closure);
    gtk_widget_add_accelerator (mainw->loop_ping_pong, "activate", mainw->accel_group,
				GDK_g, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    if (!cfile->achans||mainw->mute||mainw->loop_cont||prefs->audio_player==AUD_PLAYER_JACK||
	prefs->audio_player==AUD_PLAYER_PULSE) {
      gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), loop_closure);
      gtk_widget_add_accelerator (mainw->loop_video, "activate", mainw->accel_group,
				  GDK_l, (GdkModifierType)0,
				  GTK_ACCEL_VISIBLE);
      gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), loop_cont_closure);
      gtk_widget_add_accelerator (mainw->loop_continue, "activate", mainw->accel_group,
				  GDK_o, (GdkModifierType)0,
				  GTK_ACCEL_VISIBLE);
      gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), showfct_closure);
    }
    gtk_widget_add_accelerator (mainw->showfct, "activate", mainw->accel_group,
				GDK_h, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), showsubs_closure);
    gtk_widget_add_accelerator (mainw->showsubs, "activate", mainw->accel_group,
				GDK_v, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), sepwin_closure);
    gtk_widget_add_accelerator (mainw->sepwin, "activate", mainw->accel_group,
				GDK_s, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), dblsize_closure);
    gtk_widget_add_accelerator (mainw->dsize, "activate", mainw->accel_group,
				GDK_d, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    gtk_accel_group_disconnect (GTK_ACCEL_GROUP (mainw->accel_group), fade_closure);
    gtk_widget_add_accelerator (mainw->fade, "activate", mainw->accel_group,
				GDK_b, (GdkModifierType)0,
				GTK_ACCEL_VISIBLE);

    stop_closure=NULL;
    
  }
  if (mainw->double_size) {
    resize(2);
  }
  else {
    resize(1);
  }
}


void
fullscreen_internal(void) {
  int bx,by;

  // resize for full screen, internal player, no separate window
  
  if (mainw->multitrack==NULL) {

    gtk_widget_hide(mainw->frame1);
    gtk_widget_hide(mainw->frame2);

    gtk_widget_hide(mainw->eventbox3);
    gtk_widget_hide(mainw->eventbox4);

    gtk_frame_set_label(GTK_FRAME(mainw->playframe),NULL);
    gtk_container_set_border_width (GTK_CONTAINER (mainw->playframe), 0);

    gtk_widget_hide(mainw->t_bckground);
    gtk_widget_hide(mainw->t_double);

    gtk_widget_hide(mainw->menu_hbox);


    if (mainw->playing_file==-1) {
      gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),NULL);
    }

    while (g_main_context_iteration(NULL,FALSE));

    get_border_size(mainw->LiVES, &bx, &by);


    gtk_widget_set_size_request (mainw->playframe, lives_widget_get_allocation_width(mainw->LiVES)-bx, 
				 lives_widget_get_allocation_height(mainw->LiVES));

  }
  else {
    make_play_window();
  }

}



void 
block_expose (void) {
  // block expose/config events
  // sometimes we need to do this before showing/re-showing the play window
  // otherwise we get into a loop of expose events
  // symptoms - strace shows the app looping on poll() and it is otherwise
  // unresponsive
  g_signal_handler_block(mainw->video_draw,mainw->config_func);
  g_signal_handler_block(mainw->video_draw,mainw->vidbar_func);
  g_signal_handler_block(mainw->laudio_draw,mainw->laudbar_func);
  g_signal_handler_block(mainw->raudio_draw,mainw->raudbar_func);
}

void 
unblock_expose (void) {
  // unblock expose/config events
  g_signal_handler_unblock(mainw->video_draw,mainw->config_func);
  g_signal_handler_unblock(mainw->video_draw,mainw->vidbar_func);
  g_signal_handler_unblock(mainw->laudio_draw,mainw->laudbar_func);
  g_signal_handler_unblock(mainw->raudio_draw,mainw->raudbar_func);
}




void 
make_preview_box (void) {
  // create a box to show frames in, this will go in the sepwin when we are not playing
  GtkWidget *hbox;
  GtkWidget *hbox_rb;
  GtkWidget *hbox_buttons;
  GObject *spinbutton_adj;
  GtkWidget *radiobutton_free;
  GtkWidget *radiobutton_start;
  GtkWidget *radiobutton_end;
  GtkWidget *radiobutton_ptr;
  GSList *radiobutton_group = NULL;
  GtkWidget *hseparator;
  GtkWidget *rewind_img;
  GtkWidget *playsel_img;
  GtkWidget *play_img;
  GtkWidget *loop_img;
  GtkWidget *eventbox;
  GtkWidget *label;

  gchar buff[PATH_MAX];
  gchar *fnamex;

  mainw->preview_box = gtk_vbox_new (FALSE, 0);
  gtk_widget_show (mainw->preview_box);
  gtk_widget_ref(mainw->preview_box);

  eventbox=gtk_event_box_new();
  gtk_widget_show (eventbox);
  gtk_box_pack_start (GTK_BOX (mainw->preview_box), eventbox, TRUE, TRUE, 0);

  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
                      G_CALLBACK (frame_context),
                      GINT_TO_POINTER (3));


  mainw->preview_image = gtk_image_new_from_pixbuf (NULL);
  gtk_widget_show (mainw->preview_image);
  gtk_container_add (GTK_CONTAINER (eventbox), mainw->preview_image);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (mainw->preview_box), hbox, FALSE, FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);

  spinbutton_adj = (GObject *)gtk_adjustment_new (1., 1., cfile->frames, 1., 10., 0.);
  mainw->preview_spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1, 0);
  gtk_entry_set_width_chars (GTK_ENTRY (mainw->preview_spinbutton),8);
  gtk_widget_show (mainw->preview_spinbutton);
  gtk_box_pack_start (GTK_BOX (hbox), mainw->preview_spinbutton, FALSE, FALSE, 0);

  gtk_widget_modify_text(mainw->preview_spinbutton, GTK_STATE_INSENSITIVE, &palette->black);

  // damn thing crashes if max<min
  mainw->preview_scale=gtk_hscale_new_with_range(0,cfile->frames?cfile->frames>0:1,1);
  gtk_widget_show(mainw->preview_scale);

  gtk_box_pack_start (GTK_BOX (hbox), mainw->preview_scale, TRUE, TRUE, 0);
  gtk_range_set_adjustment(GTK_RANGE(mainw->preview_scale),GTK_ADJUSTMENT(spinbutton_adj));
  gtk_scale_set_draw_value(GTK_SCALE(mainw->preview_scale),FALSE);

  gtk_widget_set_tooltip_text( mainw->preview_spinbutton, _("Frame number to preview"));
  lives_tooltips_copy(mainw->preview_scale,mainw->preview_spinbutton);

  hbox_rb = gtk_hbox_new (FALSE, 10);
  gtk_box_pack_start (GTK_BOX (mainw->preview_box), hbox_rb, FALSE, FALSE, 0);
  gtk_widget_show (hbox_rb);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_rb), hbox, TRUE, FALSE, 0);

  radiobutton_free=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton_free), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton_free));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton_free, FALSE, FALSE, 4);

  label=gtk_label_new_with_mnemonic (_ ("_Free"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_free);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Free choice of frame number"));
  lives_tooltips_copy(radiobutton_free,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_free);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 4);

  gtk_widget_show_all (hbox);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_rb), hbox, TRUE, FALSE, 0);

  radiobutton_start=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton_start), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton_start));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton_start, FALSE, FALSE, 4);

  label=gtk_label_new_with_mnemonic (_ ("_Start"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_start);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Frame number is linked to start frame"));
  lives_tooltips_copy(radiobutton_start,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_start);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 4);

  gtk_widget_show_all (hbox);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_start), mainw->prv_link==PRV_START);


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_rb), hbox, TRUE, FALSE, 0);

  radiobutton_end=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton_end), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton_end));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton_end, FALSE, FALSE, 4);

  label=gtk_label_new_with_mnemonic (_ ("_End"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_end);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Frame number is linked to end frame"));
  lives_tooltips_copy(radiobutton_end,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_end);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 4);

  gtk_widget_show_all (hbox);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_end), mainw->prv_link==PRV_END);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox_rb), hbox, TRUE, FALSE, 0);

  radiobutton_ptr=gtk_radio_button_new(NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (radiobutton_ptr), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (radiobutton_ptr));

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton_ptr, FALSE, FALSE, 4);

  label=gtk_label_new_with_mnemonic (_ ("_Pointer"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton_ptr);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Frame number is linked to playback pointer"));
  lives_tooltips_copy(radiobutton_ptr,eventbox);
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton_ptr);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 4);

  gtk_widget_show_all (hbox);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (radiobutton_ptr), mainw->prv_link==PRV_PTR);

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (mainw->preview_box), hseparator, TRUE, TRUE, 0);

  // buttons
  hbox_buttons = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox_buttons);
  gtk_box_pack_start (GTK_BOX (mainw->preview_box), hbox_buttons, TRUE, TRUE, 0);

  rewind_img=gtk_image_new_from_stock ("gtk-media-rewind", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
  mainw->p_rewindbutton=gtk_button_new();
  gtk_widget_modify_bg (mainw->p_rewindbutton, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  gtk_button_set_relief (GTK_BUTTON (mainw->p_rewindbutton), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER(mainw->p_rewindbutton), rewind_img);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), mainw->p_rewindbutton, TRUE, TRUE, 0);
  gtk_widget_show (mainw->p_rewindbutton);
  gtk_widget_show (rewind_img);
  gtk_widget_set_tooltip_text( mainw->p_rewindbutton,_ ("Rewind"));
  gtk_widget_set_sensitive (mainw->p_rewindbutton, cfile->pointer_time>0.);

  play_img=gtk_image_new_from_stock ("gtk-media-play", gtk_toolbar_get_icon_size (GTK_TOOLBAR (mainw->btoolbar)));
  mainw->p_playbutton=gtk_button_new();
  gtk_widget_modify_bg (mainw->p_playbutton, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  gtk_button_set_relief (GTK_BUTTON (mainw->p_playbutton), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER(mainw->p_playbutton), play_img);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), mainw->p_playbutton, TRUE, TRUE, 0);
  gtk_widget_show (mainw->p_playbutton);
  gtk_widget_show (play_img);
  gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Play all"));

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"playsel.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  playsel_img=gtk_image_new_from_file (buff);
  mainw->p_playselbutton=gtk_button_new();
  gtk_widget_modify_bg (mainw->p_playselbutton, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  gtk_button_set_relief (GTK_BUTTON (mainw->p_playselbutton), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER(mainw->p_playselbutton), playsel_img);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), mainw->p_playselbutton, TRUE, TRUE, 0);
  gtk_widget_show (mainw->p_playselbutton);
  gtk_widget_show (playsel_img);
  gtk_widget_set_tooltip_text( mainw->p_playselbutton,_ ("Play Selection"));
  gtk_widget_set_sensitive (mainw->p_playselbutton, cfile->frames>0);

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  loop_img=gtk_image_new_from_file (buff);
  mainw->p_loopbutton=gtk_button_new();
  gtk_widget_modify_bg (mainw->p_loopbutton, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  gtk_button_set_relief (GTK_BUTTON (mainw->p_loopbutton), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER(mainw->p_loopbutton), loop_img);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), mainw->p_loopbutton, TRUE, TRUE, 0);
  gtk_widget_show (mainw->p_loopbutton);
  gtk_widget_show (loop_img);
  gtk_widget_set_tooltip_text( mainw->p_loopbutton,_ ("Loop On/Off"));
  gtk_widget_set_sensitive (mainw->p_loopbutton, TRUE);

  fnamex=g_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
  g_snprintf (buff,PATH_MAX,"%s",fnamex);
  g_free(fnamex);
  mainw->p_mute_img=gtk_image_new_from_file (buff);
  if (g_file_test(buff,G_FILE_TEST_EXISTS)&&!mainw->mute) {
    GdkPixbuf *pixbuf=gtk_image_get_pixbuf(GTK_IMAGE(mainw->p_mute_img));
    gdk_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->p_mutebutton=gtk_button_new();
  gtk_widget_modify_bg (mainw->p_mutebutton, GTK_STATE_ACTIVE, &palette->menu_and_bars);
  gtk_button_set_relief (GTK_BUTTON (mainw->p_mutebutton), GTK_RELIEF_NONE);
  gtk_container_add (GTK_CONTAINER(mainw->p_mutebutton), mainw->p_mute_img);
  gtk_box_pack_start (GTK_BOX (hbox_buttons), mainw->p_mutebutton, TRUE, TRUE, 0);
  gtk_widget_show (mainw->p_mutebutton);
  gtk_widget_show (mainw->p_mute_img);

  if (!mainw->mute) gtk_widget_set_tooltip_text( mainw->p_mutebutton,_("Mute the audio (z)"));
  else gtk_widget_set_tooltip_text( mainw->p_mutebutton,_("Unmute the audio (z)"));

  g_signal_connect (GTK_OBJECT (radiobutton_free), "toggled",
		    G_CALLBACK (on_prv_link_toggled),
		    GINT_TO_POINTER (PRV_FREE));
  g_signal_connect (GTK_OBJECT (radiobutton_start), "toggled",
		    G_CALLBACK (on_prv_link_toggled),
		    GINT_TO_POINTER (PRV_START));
  g_signal_connect (GTK_OBJECT (radiobutton_end), "toggled",
		    G_CALLBACK (on_prv_link_toggled),
		    GINT_TO_POINTER (PRV_END));
  g_signal_connect (GTK_OBJECT (radiobutton_ptr), "toggled",
		    G_CALLBACK (on_prv_link_toggled),
		    GINT_TO_POINTER (PRV_PTR));

  g_signal_connect (GTK_OBJECT (mainw->p_playbutton), "clicked",
                      G_CALLBACK (on_playall_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->p_playselbutton), "clicked",
                      G_CALLBACK (on_playsel_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->p_rewindbutton), "clicked",
                      G_CALLBACK (on_rewind_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->p_mutebutton), "clicked",
                      G_CALLBACK (on_mute_button_activate),
                      NULL);
  g_signal_connect (GTK_OBJECT (mainw->p_loopbutton), "clicked",
                      G_CALLBACK (on_loop_button_activate),
                      NULL);

  mainw->preview_spin_func=g_signal_connect_after (GTK_OBJECT (mainw->preview_spinbutton), "value_changed",
						   G_CALLBACK (on_preview_spinbutton_changed),
						   NULL);

}


void 
enable_record (void) {
  set_menu_text (mainw->record_perf, _("Start _recording"),TRUE);
  gtk_widget_set_sensitive (mainw->record_perf, TRUE);
}

void 
toggle_record (void) {
  set_menu_text (mainw->record_perf, _("Stop _recording"),TRUE);
}


void 
disable_record (void) {
  set_menu_text (mainw->record_perf, _("_Record Performance"),TRUE);
}


void make_play_window(void) {
  //  separate window

  gchar *xtrabit;
  gchar *title;

  if (mainw->playing_file>-1) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

  gtk_image_set_from_pixbuf(GTK_IMAGE(mainw->image274),NULL);

  if (mainw->play_window!=NULL) {
    // this shouldn't ever happen
    kill_play_window();
  } 
  
  mainw->play_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_window_set_position(GTK_WINDOW(mainw->play_window),GTK_WIN_POS_CENTER_ALWAYS);

  if (mainw->multitrack==NULL) gtk_window_add_accel_group (GTK_WINDOW (mainw->play_window), mainw->accel_group);
  else gtk_window_add_accel_group (GTK_WINDOW (mainw->play_window), mainw->multitrack->accel_group);

  gtk_widget_modify_bg (mainw->play_window, GTK_STATE_NORMAL, &palette->normal_back);

  // show the window (so we can hide its cursor !), and get its xwin
  if (!(mainw->fs&&mainw->playing_file>-1&&mainw->vpp!=NULL)) {
    gtk_widget_show(mainw->play_window);
  }

  resize_play_window();
  if (mainw->play_window==NULL) return;

  if (mainw->playing_file==-1&&mainw->current_file>0&&cfile->frames>0&&mainw->multitrack==NULL) {
    if (mainw->preview_box==NULL) {
      // create the preview box that shows frames
      make_preview_box();
    }
    if (cfile->is_loaded) {
      //and add it the play window
      gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
      gtk_widget_set_size_request(mainw->preview_box,-1,PREVIEW_BOX_HT);

      if (mainw->is_processing&&!cfile->nopreview) gtk_widget_set_tooltip_text( mainw->p_playbutton,_ ("Preview")); 
      gtk_widget_grab_focus (mainw->preview_scale);

      if (mainw->is_processing&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
	// block spinbutton in play window
	gtk_widget_set_sensitive(mainw->preview_spinbutton,FALSE);
      }

      // load whatever frame and show it
      if (mainw->prv_link!=PRV_FREE) mainw->preview_frame=0;
      load_preview_image(FALSE);
      // force a redraw
      gtk_widget_queue_resize(mainw->preview_box);
      // be careful, the user could switch out of sepwin here !
      mainw->noswitch=TRUE;

      while (g_main_context_iteration(NULL,FALSE));
      mainw->noswitch=FALSE;
      if (mainw->play_window==NULL) return;
   }
  }


  if (mainw->sepwin_scale!=100.) xtrabit=g_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
  else xtrabit=g_strdup("");

  if (mainw->playing_file>-1) {
    title=g_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
    gtk_window_set_title (GTK_WINDOW (mainw->play_window), title);
  }
  else {
    title=g_strdup_printf("%s%s",gtk_window_get_title(GTK_WINDOW
						      (mainw->multitrack==NULL?mainw->LiVES:
						       mainw->multitrack->window)),xtrabit);
    gtk_window_set_title(GTK_WINDOW(mainw->play_window),title);
  }
  g_free(title);
  g_free(xtrabit);

  if ((mainw->current_file==-1||(!cfile->is_loaded&&!mainw->preview)||
       (cfile->frames==0&&(mainw->multitrack==NULL||mainw->playing_file==-1)))&&mainw->imframe!=NULL) {
    cairo_t *cr = gdk_cairo_create (lives_widget_get_xwindow(mainw->play_window));
    gdk_cairo_set_source_pixbuf (cr, mainw->imframe, (GdkModifierType)0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);
  }

  gtk_widget_set_tooltip_text( mainw->m_sepwinbutton,_ ("Hide Play Window"));

  mainw->pw_exp_func=g_signal_connect_after (GTK_OBJECT (mainw->play_window), "expose_event",
					     G_CALLBACK (expose_play_window),
					     NULL);
  
  g_signal_connect (GTK_OBJECT (mainw->play_window), "delete_event",
		    G_CALLBACK (on_stop_activate_by_del),
		    NULL);
  
  
  if (((mainw->current_file>-1&&(cfile->is_loaded||(cfile->clip_type!=CLIP_TYPE_DISK&&
						    cfile->clip_type!=CLIP_TYPE_FILE)))||
       (mainw->preview&&cfile->frames>0))&&(mainw->multitrack!=NULL||mainw->playing_file>-1)) {
    g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
    mainw->pw_exp_is_blocked=TRUE;
  }
  else mainw->pw_exp_is_blocked=FALSE;


  if (!mainw->ext_playback) {
    // be careful, the user could switch out of sepwin here !
    if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) g_source_remove(mainw->multitrack->idlefunc);
    mainw->noswitch=TRUE;

    if (mainw->current_file>-1) {
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }

    while (g_main_context_iteration(NULL,FALSE));
    if (mainw->multitrack!=NULL&&mainw->multitrack->idlefunc>0) {
      mainw->multitrack->idlefunc=0;
      mainw->multitrack->idlefunc=mt_idle_add(mainw->multitrack);
    }
    mainw->noswitch=FALSE;
    if (mainw->play_window==NULL) return;
  }

  g_signal_connect (GTK_OBJECT (mainw->play_window), "scroll_event",
		    G_CALLBACK (on_mouse_scroll),
		    NULL);

}



void resize_play_window (void) {
  gint opwx,opwy,pmonitor=prefs->play_monitor;

  gboolean fullscreen=TRUE;
  gboolean size_ok;

  int xht;

  uint64_t xwinid=0;

  mainw->sepwin_scale=100.;

#ifdef DEBUG_HANGS
  fullscreen=FALSE;
#endif

  if (mainw->play_window==NULL) return;

  if ((mainw->current_file==-1||(cfile->frames==0&&mainw->multitrack==NULL)||
       (!cfile->is_loaded&&!mainw->preview&&cfile->clip_type!=CLIP_TYPE_GENERATOR))||
      (mainw->multitrack!=NULL&&mainw->playing_file<1&&!mainw->preview)) {
    if (mainw->imframe!=NULL) {
      mainw->pwidth=lives_pixbuf_get_width (mainw->imframe);
      mainw->pheight=lives_pixbuf_get_height (mainw->imframe);
    }
    else {
      if (mainw->multitrack==NULL) {
	mainw->pwidth=DEFAULT_FRAME_HSIZE;
	mainw->pheight=DEFAULT_FRAME_VSIZE;
      }
    }
  }
  else {
    if (mainw->multitrack==NULL) {
      mainw->pwidth=cfile->hsize;
      mainw->pheight=cfile->vsize;
    }
    else {
      mainw->pwidth=mainw->files[mainw->multitrack->render_file]->hsize;
      mainw->pheight=mainw->files[mainw->multitrack->render_file]->vsize;
      mainw->must_resize=TRUE;
    }

    size_ok=FALSE;

    do {
      if (pmonitor==0) {
	if (mainw->pwidth>mainw->scr_width-DSIZE_SAFETY_H||
	    mainw->pheight>mainw->scr_height-DSIZE_SAFETY_V) { 
	  mainw->pheight=(mainw->pheight>>2)<<1;
	  mainw->pwidth=(mainw->pwidth>>2)<<1;
	  mainw->sepwin_scale/=2.;
	}
	else size_ok=TRUE;
      }
      else {
	if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-DSIZE_SAFETY_H||
	    mainw->pheight>mainw->mgeom[pmonitor-1].height-DSIZE_SAFETY_V) {
	  mainw->pheight=(mainw->pheight>>2)<<1;
	  mainw->pwidth=(mainw->pwidth>>2)<<1;
	  mainw->sepwin_scale/=2.;
	}
	else size_ok=TRUE;
      }
    } while (!size_ok);
  }
 
  if (mainw->playing_file>-1) {
    if (mainw->double_size&&mainw->multitrack==NULL) {
      mainw->pheight*=2;
      mainw->pwidth*=2;
      if (pmonitor==0) {
	if (mainw->pwidth>mainw->scr_width-DSIZE_SAFETY_H||mainw->pheight>mainw->scr_height-DSIZE_SAFETY_V) {
	  calc_maxspect(mainw->scr_width-DSIZE_SAFETY_H,mainw->scr_height-DSIZE_SAFETY_V,&mainw->pwidth,&mainw->pheight);
	  mainw->sepwin_scale=(float)mainw->pwidth/(float)cfile->hsize*100.;
	}
      }
      else {
	if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-DSIZE_SAFETY_H||
	    mainw->pheight>mainw->mgeom[pmonitor-1].height-DSIZE_SAFETY_V) { 
	  calc_maxspect(mainw->mgeom[pmonitor-1].width-DSIZE_SAFETY_H,mainw->mgeom[pmonitor-1].height-DSIZE_SAFETY_V,
			&mainw->pwidth,&mainw->pheight);
	  mainw->sepwin_scale=(float)mainw->pwidth/(float)cfile->hsize*100.;
	}
      }
    }

    if (mainw->fs) {
      if (!lives_widget_is_visible (mainw->play_window)) {
	gtk_widget_show (mainw->play_window);
	// be careful, the user could switch out of sepwin here !
	mainw->noswitch=TRUE;
	while (g_main_context_iteration (NULL,FALSE));
	mainw->noswitch=FALSE;
	if (mainw->play_window==NULL) return;
	if (!mainw->fs||mainw->playing_file<0) goto point1;
	mainw->opwx=mainw->opwy=-1;
      }
      else {
	if (pmonitor==0) {
	  mainw->opwx=(mainw->scr_width-mainw->pwidth)/2;
	  mainw->opwy=(mainw->scr_height-mainw->pheight)/2;
	}
	else {
	  mainw->opwx=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
	  mainw->opwy=mainw->mgeom[pmonitor-1].y+(mainw->mgeom[pmonitor-1].height-mainw->pheight)/2;
	}
      }

      if (pmonitor==0) {
	mainw->pwidth=mainw->scr_width;
	mainw->pheight=mainw->scr_height;
      }
      else {
	mainw->pwidth=mainw->mgeom[pmonitor-1].width;
	mainw->pheight=mainw->mgeom[pmonitor-1].height;
      }

      if (lives_widget_is_visible (mainw->play_window)) {
	// store old postion of window
	gtk_window_get_position (GTK_WINDOW (mainw->play_window),&opwx,&opwy);
	if (opwx*opwy) {
	  mainw->opwx=opwx;
	  mainw->opwy=opwy;
	}
      }

      if (pmonitor==0) {
	if (mainw->vpp!=NULL&&mainw->vpp->fwidth>0) {
	  gtk_window_move (GTK_WINDOW (mainw->play_window), (mainw->scr_width-mainw->vpp->fwidth)/2,
			   (mainw->scr_height-mainw->vpp->fheight)/2);
	}
	else gtk_window_move (GTK_WINDOW (mainw->play_window), (GdkModifierType)0, 0);
      }
      else {
	gtk_window_set_screen(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
	if (mainw->vpp!=NULL&&mainw->vpp->fwidth>0) {
	  gtk_window_move (GTK_WINDOW (mainw->play_window), mainw->mgeom[pmonitor-1].x+
			   (mainw->mgeom[pmonitor-1].width-mainw->vpp->fwidth)/2,
			   mainw->mgeom[pmonitor-1].y+(mainw->mgeom[pmonitor-1].height-mainw->vpp->fheight)/2);
	}
	else gtk_window_move(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].x,mainw->mgeom[pmonitor-1].y);
      }
      
      // leave this alone * !
      if (!(mainw->vpp!=NULL&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY))) {
	gtk_window_fullscreen(GTK_WINDOW(mainw->play_window));
	gtk_window_resize (GTK_WINDOW (mainw->play_window), mainw->pwidth, mainw->pheight);
	gtk_widget_queue_resize (mainw->play_window);
      }

      // init the playback plugin, unless there is a possibility of wrongly sized frames (i.e. during a preview)
      if (mainw->vpp!=NULL&&(!mainw->preview||mainw->multitrack!=NULL)) {
	gboolean fixed_size=FALSE;

	gdk_window_get_pointer (gdk_get_default_root_window (), &mainw->ptr_x, &mainw->ptr_y, NULL);
	if (pmonitor==0) mainw->ptr_x=mainw->ptr_y=-1;
	if (mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {	  
	  // fixed o/p size for stream
	  if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
	    if (mainw->current_file>-1) {
	      mainw->vpp->fwidth=cfile->hsize;
	      mainw->vpp->fheight=cfile->vsize;
	    }
	    else mainw->vpp->fwidth=mainw->vpp->fheight=-1;
	  }
	  mainw->pwidth=mainw->vpp->fwidth;
	  mainw->pheight=mainw->vpp->fheight;
	  fixed_size=TRUE;

	  // * leave this alone !
	  gtk_window_unfullscreen(GTK_WINDOW(mainw->play_window));

	  if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)) 
	    gtk_window_set_title (GTK_WINDOW (mainw->play_window),_("LiVES: - Streaming"));

	  gtk_window_resize (GTK_WINDOW (mainw->play_window), mainw->pwidth, mainw->pheight);
	  gtk_widget_queue_resize (mainw->play_window);
	}

	if (pmonitor!=0) {
	  fullscreen=FALSE;
	  if (mainw->play_window!=NULL) {
#ifdef USE_X11
	    xwinid=(uint64_t)GDK_WINDOW_XID(lives_widget_get_xwindow(mainw->play_window));
#else
	    xwinid=(uint64_t)gdk_win32_drawable_get_handle (lives_widget_get_xwindow(mainw->play_window));
#endif
	  }
	}
	if (mainw->ext_playback) {
	  mainw->ext_keyboard=FALSE;
#ifdef RT_AUDIO
	  stop_audio_stream();
#endif
	  if (mainw->vpp->exit_screen!=NULL) {
	    (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
	  }
	  if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY&&pmonitor==0) 
	    gtk_window_set_keep_below(GTK_WINDOW(mainw->play_window),FALSE);
	}

#ifdef RT_AUDIO
	if (mainw->vpp->audio_codec!=AUDIO_CODEC_NONE&&prefs->stream_audio_out) {
	  start_audio_stream();
	}
	else {
	  clear_audio_stream();
	}
#endif

	if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY&&pmonitor==0) 
	  gtk_window_set_keep_below(GTK_WINDOW(mainw->play_window),TRUE);

	if ((mainw->vpp->init_screen==NULL)||((*mainw->vpp->init_screen)
					      (mainw->pwidth,mainw->pheight*(fixed_size?1:prefs->virt_height),
					       fullscreen,xwinid,mainw->vpp->extra_argc,mainw->vpp->extra_argv))) {
	  mainw->ext_playback=TRUE;
	  // the play window is still visible (in case it was 'always on top')
	  // start key polling from ext plugin
	  if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY&&pmonitor==0) {
	    mainw->ext_keyboard=TRUE;
	    return;
	  }
	}
      }
    }
    else {

    point1:
      if (mainw->playing_file==0) {
	mainw->pheight=clipboard->vsize;
	mainw->pwidth=clipboard->hsize;

	size_ok=FALSE;
	
	do {
	  if (pmonitor==0) {
	    if (mainw->pwidth>mainw->scr_width-DSIZE_SAFETY_H||
		mainw->pheight>mainw->scr_height-DSIZE_SAFETY_V) { 
	      mainw->pheight=(mainw->pheight>>2)<<1;
	      mainw->pwidth=(mainw->pwidth>>2)<<1;
	    }
	    else size_ok=TRUE;
	  }
	  else {
	    if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-DSIZE_SAFETY_H||
		mainw->pheight>mainw->mgeom[pmonitor-1].height-DSIZE_SAFETY_V) {
	      mainw->pheight=(mainw->pheight>>2)<<1;
	      mainw->pwidth=(mainw->pwidth>>2)<<1;
	    }
	    else size_ok=TRUE;
	  }
	} while (!size_ok);
      }
      if (pmonitor==0) gtk_window_move (GTK_WINDOW (mainw->play_window), (mainw->scr_width-mainw->pwidth)/2, 
					(mainw->scr_height-mainw->pheight)/2);
      else {
	gint xcen=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
	gtk_window_set_screen(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
	gtk_window_move (GTK_WINDOW (mainw->play_window), xcen, (mainw->scr_height-mainw->pheight)/2);
      }
    }
    gtk_window_present (GTK_WINDOW (mainw->play_window));
    gdk_window_raise(lives_widget_get_xwindow(mainw->play_window));
  }
  else {
    // not playing
    if (mainw->fs&&mainw->playing_file==-2&&mainw->sep_win&&prefs->sepwin_type==1) {
      if (mainw->opwx>=0&&mainw->opwy>=0) {
	// move window back to its old position after play
	if (pmonitor>0) gtk_window_set_screen(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
	gtk_window_move (GTK_WINDOW (mainw->play_window), mainw->opwx, mainw->opwy);
      }
      else {
	if (pmonitor==0) gtk_window_move (GTK_WINDOW (mainw->play_window), (mainw->scr_width-mainw->pwidth)/2, 
					  (mainw->scr_height-mainw->pheight-PREVIEW_BOX_HT*2)/2);
	else {
	  gint xcen=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
	  gtk_window_set_screen(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
	  gtk_window_move (GTK_WINDOW (mainw->play_window), xcen, (mainw->scr_height-mainw->pheight-PREVIEW_BOX_HT*2)/2);
	}
      }
    }
    else {
      if (pmonitor==0) gtk_window_move (GTK_WINDOW (mainw->play_window), (mainw->scr_width-mainw->pwidth)/2, 
					(mainw->scr_height-mainw->pheight-PREVIEW_BOX_HT*2)/2);
      else {
	gint xcen=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
	gtk_window_set_screen(GTK_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
	gtk_window_move (GTK_WINDOW (mainw->play_window), xcen, (mainw->scr_height-mainw->pheight-PREVIEW_BOX_HT*2)/2);
      }
    }
    mainw->opwx=mainw->opwy=-1;
  }

  gtk_window_resize (GTK_WINDOW (mainw->play_window), mainw->pwidth, mainw->pheight);

  // workaround for gtk+ lameness
  if (mainw->playing_file<0&&(mainw->current_file>-1&&!cfile->opening)) xht=PREVIEW_BOX_HT;
  else xht=0;
  
  gtk_widget_set_size_request (mainw->play_window, mainw->pwidth, mainw->pheight+xht);
  

}



void
kill_play_window (void) {
  // plug our player back into internal window

  if (mainw->play_window!=NULL) {
    if (!mainw->pw_exp_is_blocked)
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
    if (mainw->preview_box!=NULL&&lives_widget_get_parent(mainw->preview_box)!=NULL) {
      // preview_box is refed, so it will survive
      gtk_container_remove (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
    }
    if (GTK_IS_WINDOW (mainw->play_window )) gtk_widget_destroy(mainw->play_window);
    mainw->play_window=NULL;
  }
  gtk_widget_set_tooltip_text( mainw->m_sepwinbutton,_("Show Play Window"));
}



void 
add_to_playframe (void) {
  // plug the playback image into its frame in the main window

  gtk_widget_show(mainw->playarea);

  ///////////////////////////////////////////////////
  if (mainw->plug==NULL) {
    if (!mainw->foreign&&(!mainw->sep_win||prefs->sepwin_type==0)) {
      mainw->plug = gtk_hbox_new (FALSE,0);
      gtk_container_add(GTK_CONTAINER(mainw->playarea),mainw->plug);
      gtk_widget_modify_bg (mainw->plug, GTK_STATE_NORMAL, &palette->normal_back);
      gtk_widget_show (mainw->plug);
      gtk_container_add (GTK_CONTAINER (mainw->plug), mainw->image274);
    }
  }
}


LIVES_INLINE void frame_size_update(void) {
  // update widgets when the frame size changes
  on_double_size_activate(NULL,GINT_TO_POINTER(1));
}






//////////////////////////////////////////////////////////////////////

// splash screen



void splash_init(void) {
  GtkWidget *vbox,*hbox;
  GtkWidget *splash_img;
  GdkPixbuf *splash_pix;

  GError *error=NULL;
  gchar *tmp=g_strdup_printf("%s/%s/lives-splash.png",prefs->prefix_dir,THEME_DIR);

  gtk_window_set_auto_startup_notification(FALSE);

  mainw->splash_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  if (prefs->gui_monitor!=0) {
    gtk_window_set_screen(GTK_WINDOW(mainw->splash_window),mainw->mgeom[prefs->gui_monitor-1].screen);
  }

  gtk_window_set_position(GTK_WINDOW(mainw->splash_window),GTK_WIN_POS_CENTER_ALWAYS);

  gtk_window_set_type_hint(GTK_WINDOW(mainw->splash_window),GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(mainw->splash_window, GTK_STATE_NORMAL, &palette->normal_back);
  }

  
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_add (GTK_CONTAINER (mainw->splash_window), vbox);

  splash_pix=gdk_pixbuf_new_from_file(tmp,&error);
  g_free(tmp);

  splash_img = gtk_image_new_from_pixbuf (splash_pix);

  gtk_box_pack_start (GTK_BOX (vbox), splash_img, TRUE, TRUE, 0);

  if (splash_pix!=NULL) lives_object_unref(splash_pix);


  mainw->splash_label=gtk_label_new("");

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->splash_label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_box_pack_start (GTK_BOX (vbox), mainw->splash_label, TRUE, TRUE, 0);

  mainw->splash_progress = gtk_progress_bar_new ();
  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(mainw->splash_progress),.01);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(mainw->splash_progress, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hbox = gtk_hbox_new (FALSE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), mainw->splash_progress, TRUE, TRUE, 20);

  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 20);



  gtk_widget_show_all(mainw->splash_window);

  gtk_window_present(GTK_WINDOW(mainw->splash_window));


  while (g_main_context_iteration(NULL,FALSE));

  lives_set_cursor_style(LIVES_CURSOR_BUSY,lives_widget_get_xwindow(mainw->splash_window));

  gtk_window_set_auto_startup_notification(TRUE);



}




void splash_msg(const gchar *msg, gdouble pct) {

  if (mainw->foreign||mainw->splash_window==NULL) return;

  gtk_label_set_text(GTK_LABEL(mainw->splash_label),msg);

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(mainw->splash_progress),pct);

  gtk_widget_queue_draw(mainw->splash_window);

  while (g_main_context_iteration(NULL,FALSE));

}





void splash_end(void) {
  
  if (mainw->foreign) return;
    
  if (mainw->splash_window!=NULL) {

    end_threaded_dialog();
    
    gtk_widget_destroy(mainw->splash_window);
    
    mainw->splash_window=NULL;
  }

  if (prefs->startup_interface==STARTUP_MT&&mainw->multitrack==NULL) on_multitrack_activate(NULL,NULL);

}

