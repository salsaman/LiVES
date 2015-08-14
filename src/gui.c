// gui.c
// LiVES
// (c) G. Finch 2004 - 2014 <salsaman@gmail.com>
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
#include "ce_thumbs.h"

#ifdef ENABLE_GIW_3
#include "giw/giwtimeline.h"
#endif

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
static LiVESWidgetClosure *stop_closure;
static LiVESWidgetClosure *fullscreen_closure;
static LiVESWidgetClosure *dblsize_closure;
static LiVESWidgetClosure *sepwin_closure;
static LiVESWidgetClosure *loop_closure;
static LiVESWidgetClosure *loop_cont_closure;
static LiVESWidgetClosure *fade_closure;
static LiVESWidgetClosure *showfct_closure;
static LiVESWidgetClosure *showsubs_closure;
static LiVESWidgetClosure *rec_closure;
static LiVESWidgetClosure *mute_audio_closure;
static LiVESWidgetClosure *ping_pong_closure;


void load_theme(void) {
  // load the theme images
  // TODO - set palette in here ?
  LiVESError *error=NULL;
  char *tmp=lives_build_filename(prefs->prefix_dir,THEME_DIR,prefs->theme,"main.jpg",NULL);
  mainw->imsep=lives_pixbuf_new_from_file(tmp,&error);
  lives_free(tmp);

  if (!(error==NULL)) {
    palette->style=STYLE_PLAIN;
    lives_snprintf(prefs->theme,64,"%%ERROR%%");
    lives_error_free(error);
  } else {
    mainw->sep_image = lives_image_new_from_pixbuf(mainw->imsep);
    tmp=lives_build_filename(prefs->prefix_dir,THEME_DIR,prefs->theme,"frame.jpg",NULL);
    mainw->imframe=lives_pixbuf_new_from_file(tmp,&error);
    lives_free(tmp);
    if (!(error==NULL)) {
      lives_error_free(error);
    }
  }
}


void add_message_scroller(LiVESWidget *conter) {
  LiVESTextBuffer *tbuff=NULL;
  char *all_text=NULL;

  if (mainw->textview1!=NULL) {
    all_text=lives_text_view_get_text(LIVES_TEXT_VIEW(mainw->textview1));
    lives_widget_destroy(mainw->textview1);
  }

  if (mainw->scrolledwindow!=NULL) {
    lives_widget_destroy(mainw->scrolledwindow);
  }

  mainw->scrolledwindow = lives_scrolled_window_new(NULL, NULL);
  lives_scrolled_window_set_policy(LIVES_SCROLLED_WINDOW(mainw->scrolledwindow),LIVES_POLICY_AUTOMATIC,LIVES_POLICY_ALWAYS);
  lives_widget_show(mainw->scrolledwindow);
  lives_widget_set_vexpand(mainw->scrolledwindow,TRUE);

  lives_container_add(LIVES_CONTAINER(conter), mainw->scrolledwindow);

  mainw->textview1 = lives_text_view_new();
  lives_widget_show(mainw->textview1);
  lives_container_add(LIVES_CONTAINER(mainw->scrolledwindow), mainw->textview1);

  tbuff=lives_text_view_get_buffer(LIVES_TEXT_VIEW(mainw->textview1));
  if (tbuff!=NULL && all_text!=NULL) {
    lives_text_buffer_set_text(tbuff,all_text,-1);
    lives_free(all_text);
    lives_text_view_scroll_onscreen(LIVES_TEXT_VIEW(mainw->textview1));
  }

  lives_widget_set_size_request(mainw->textview1, -1, MSG_AREA_HEIGHT);
  lives_text_view_set_editable(LIVES_TEXT_VIEW(mainw->textview1), FALSE);
  lives_text_view_set_wrap_mode(LIVES_TEXT_VIEW(mainw->textview1), LIVES_WRAP_WORD);
  lives_text_view_set_cursor_visible(LIVES_TEXT_VIEW(mainw->textview1), FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_base_color(mainw->textview1, LIVES_WIDGET_STATE_NORMAL, &palette->info_base);
    lives_widget_set_text_color(mainw->textview1, LIVES_WIDGET_STATE_NORMAL, &palette->info_text);
  }
}


void make_custom_submenus(void) {
  mainw->custom_gens_submenu = lives_menu_item_new_with_mnemonic(_("_Custom Generators"));
  mainw->custom_effects_submenu = lives_menu_item_new_with_mnemonic(_("_Custom Effects"));
  mainw->custom_utilities_submenu = lives_menu_item_new_with_mnemonic(_("Custom _Utilities"));

}

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_sim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  int current_file=mainw->current_file;
  if (current_file>-1&&cfile!=NULL&&cfile->cb_src!=-1) mainw->current_file=cfile->cb_src;
  if (mainw->current_file>0&&cfile!=NULL) {
    load_start_image(cfile->start);
  } else load_start_image(0);
  mainw->current_file=current_file;
  return TRUE;
}

boolean expose_eim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  int current_file=mainw->current_file;
  if (current_file>-1&&cfile!=NULL&&cfile->cb_src!=-1) mainw->current_file=cfile->cb_src;
  if (mainw->current_file>0&&cfile!=NULL) {
    load_end_image(cfile->end);
  } else load_end_image(0);
  mainw->current_file=current_file;
  return TRUE;
}

boolean expose_pim(LiVESWidget *widget, lives_painter_t *cr, livespointer user_data) {
  int current_file=mainw->current_file;
  if (current_file>-1&&cfile!=NULL&&cfile->cb_src!=-1) mainw->current_file=cfile->cb_src;
  if (!mainw->draw_blocked) {
    load_preview_image(FALSE);
  }
  mainw->current_file=current_file;
  return TRUE;
}
#endif

void create_LiVES(void) {
  LiVESWidget *hbox1;
  LiVESWidget *vbox2;
  LiVESWidget *menuitem;
  LiVESWidget *menuitem_menu;
  LiVESWidget *separatormenuitem;
  LiVESWidget *select_submenu_menu;
  LiVESWidget *submenu_menu;
  LiVESWidget *export_submenu_menu;
  LiVESWidget *trimaudio_submenu_menu;
  LiVESWidget *delaudio_submenu_menu;
  LiVESWidget *menuitemsep;
#if LIVES_HAS_IMAGE_MENU_ITEM
  LiVESWidget *image;
#endif
  LiVESWidget *effects;
  LiVESWidget *tools;
  LiVESWidget *audio;
  LiVESWidget *audio_menu;
  LiVESWidget *info;
  LiVESWidget *info_menu;
  LiVESWidget *advanced;
  LiVESWidget *advanced_menu;
  LiVESWidget *rfx_menu;
  LiVESWidget *rfx_submenu;
  LiVESWidget *midi_menu;
  LiVESWidget *midi_submenu;
  LiVESWidget *midi_learn;
  LiVESWidget *midi_load;
  LiVESWidget *midi_save;
  LiVESWidget *vj_menu;
  LiVESWidget *toys_menu;
  LiVESWidget *win;
  LiVESWidget *about;
  LiVESWidget *show_manual;
  LiVESWidget *email_author;
  LiVESWidget *donate;
  LiVESWidget *report_bug;
  LiVESWidget *suggest_feature;
  LiVESWidget *help_translate;
  LiVESWidget *vbox4;
  LiVESWidget *pf_label;
  LiVESWidget *label;
  LiVESWidget *hbox3;
  LiVESWidget *t_label;
  LiVESWidget *eventbox;

#if defined (HAVE_YUV4MPEG) || defined (HAVE_UNICAP)
  LiVESWidget *submenu;
#endif

  LiVESWidget *new_test_rfx;
  LiVESWidget *copy_rfx;
  LiVESWidget *import_custom_rfx;
  LiVESWidget *rebuild_rfx;
  LiVESWidget *assign_rte_keys;

  LiVESWidget *tmp_toolbar_icon;

  LiVESAdjustment *adj;

  LiVESWidgetColor normal;

  LiVESPixbuf *pixbuf;

  char buff[32768];

  char *tmp;
  char *fnamex;

  int dpw;
  boolean woat;

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

  mainw->sep_image = lives_image_new_from_pixbuf(NULL);
  mainw->start_image = lives_image_new_from_pixbuf(NULL);
  mainw->end_image = lives_image_new_from_pixbuf(NULL);
  mainw->imframe=mainw->imsep=NULL;

  lives_widget_show(mainw->start_image);
  lives_widget_show(mainw->end_image);

  if (palette->style&STYLE_1) {
    load_theme();
  }


#if GTK_CHECK_VERSION(3,0,0)
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->start_image), LIVES_WIDGET_EXPOSE_EVENT,
                       LIVES_GUI_CALLBACK(expose_sim),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->end_image), LIVES_WIDGET_EXPOSE_EVENT,
                       LIVES_GUI_CALLBACK(expose_eim),
                       NULL);
#else

  if (mainw->imframe!=NULL) {
    lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->start_image),mainw->imframe);
    lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->end_image),mainw->imframe);
  }

#endif

  mainw->accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  mainw->layout_textbuffer=lives_text_buffer_new();
  lives_object_ref(mainw->layout_textbuffer);
  mainw->affected_layouts_map=NULL;

  mainw->LiVES = lives_window_new(LIVES_WINDOW_TOPLEVEL);
  lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(mainw->LiVES),FALSE);


  if (prefs->present&&prefs->show_gui)
    lives_window_present(LIVES_WINDOW(mainw->LiVES));

#ifdef GUI_GTK
  // TODO - can we use just DEFAULT_DROP ?
  gtk_drag_dest_set(mainw->LiVES,GTK_DEST_DEFAULT_ALL,mainw->target_table,2,
                    (GdkDragAction)(GDK_ACTION_COPY|GDK_ACTION_MOVE|GDK_ACTION_LINK));

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->LiVES), LIVES_WIDGET_DRAG_DATA_RECEIVED_SIGNAL,
                       LIVES_GUI_CALLBACK(drag_from_outside),
                       NULL);

#endif

  if (capable->smog_version_correct) lives_window_set_decorated(LIVES_WINDOW(mainw->LiVES),prefs->open_decorated);

  if (palette->style==STYLE_PLAIN) {
    // if lives_widget_ensure_style is used, we can't grab external frames...
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3,0,0)
    if (!mainw->foreign) {
      gtk_widget_ensure_style(mainw->LiVES);
    }
#endif
#endif
    lives_widget_get_bg_color(mainw->LiVES,&normal);
    lives_widget_color_copy((LiVESWidgetColor *)(&palette->normal_back),&normal);

    lives_widget_get_fg_color(mainw->LiVES,&normal);
    lives_widget_color_copy((LiVESWidgetColor *)(&palette->normal_fore),&normal);
  }

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->LiVES, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->LiVES, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_window_set_title(LIVES_WINDOW(mainw->LiVES), "LiVES");

  mainw->vbox1 = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(mainw->LiVES), mainw->vbox1);
  lives_widget_show(mainw->vbox1);

  mainw->menu_hbox = lives_hbox_new(FALSE, 0);
  lives_widget_show(mainw->menu_hbox);
  lives_box_pack_start(LIVES_BOX(mainw->vbox1), mainw->menu_hbox, FALSE, FALSE, 0);

  mainw->menubar = lives_menu_bar_new();
  lives_widget_show(mainw->menubar);
  lives_box_pack_start(LIVES_BOX(mainw->menu_hbox), mainw->menubar, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->menubar, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->menubar, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  menuitem = lives_menu_item_new_with_mnemonic(_("_File"));
  lives_widget_show(menuitem);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  menuitem_menu = lives_menu_new();
  lives_menu_set_accel_group(LIVES_MENU(menuitem_menu),mainw->accel_group);

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
    lives_widget_set_bg_color(menuitem_menu, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
  }

  mainw->open = lives_menu_item_new_with_mnemonic(_("_Open File/Directory"));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open);
  lives_widget_add_accelerator(mainw->open, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_o, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->open_sel = lives_menu_item_new_with_mnemonic(_("O_pen Part of File..."));

  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open_sel);



  mainw->open_loc = lives_menu_item_new_with_mnemonic(_("Open _Location/Stream..."));

#ifdef HAVE_WEBM

  mainw->open_loc_menu = lives_menu_item_new_with_mnemonic(_("Open _Location/Stream..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open_loc_menu);

  mainw->open_loc_submenu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_loc_menu), mainw->open_loc_submenu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->open_loc_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->open_loc_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->open_utube = lives_menu_item_new_with_mnemonic(_("Open _Youtube Clip..."));
  lives_container_add(LIVES_CONTAINER(mainw->open_loc_submenu), mainw->open_utube);

  lives_container_add(LIVES_CONTAINER(mainw->open_loc_submenu), mainw->open_loc);

#else

  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open_loc);

#endif

  mainw->open_vcd_menu = lives_menu_item_new_with_mnemonic(_("Import from _dvd/vcd..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open_vcd_menu);
  mainw->open_vcd_submenu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_vcd_menu), mainw->open_vcd_submenu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->open_vcd_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->open_vcd_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->open_dvd = lives_menu_item_new_with_mnemonic(_("Import from _dvd"));
  lives_container_add(LIVES_CONTAINER(mainw->open_vcd_submenu), mainw->open_dvd);

  mainw->open_vcd = lives_menu_item_new_with_mnemonic(_("Import from _vcd"));
  lives_container_add(LIVES_CONTAINER(mainw->open_vcd_submenu), mainw->open_vcd);

  mainw->open_device_menu = lives_menu_item_new_with_mnemonic(_("_Import from Firewire"));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->open_device_menu);
  mainw->open_device_submenu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_device_menu), mainw->open_device_submenu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->open_device_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->open_device_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->open_firewire = lives_menu_item_new_with_mnemonic(_("Import from _Firewire Device (dv)"));
  mainw->open_hfirewire = lives_menu_item_new_with_mnemonic(_("Import from _Firewire Device (hdv)"));

#ifdef HAVE_LDVGRAB
  lives_container_add(LIVES_CONTAINER(mainw->open_device_submenu), mainw->open_firewire);
  lives_container_add(LIVES_CONTAINER(mainw->open_device_submenu), mainw->open_hfirewire);
  lives_widget_show(mainw->open_firewire);
  lives_widget_show(mainw->open_hfirewire);
  lives_widget_show(mainw->open_device_menu);
  lives_widget_show(mainw->open_device_submenu);
#endif

  lives_widget_show(mainw->open);

  if (capable->has_mplayer||capable->has_mplayer2) {
    lives_widget_show(mainw->open_sel);
#ifdef ENABLE_DVD_GRAB
    lives_widget_show(mainw->open_vcd_menu);
    lives_widget_show(mainw->open_vcd_submenu);
    lives_widget_show(mainw->open_dvd);
    lives_widget_show(mainw->open_vcd);
#endif
#ifdef HAVE_WEBM
    lives_widget_show_all(mainw->open_loc_menu);
#else
    lives_widget_show(mainw->open_loc);
#endif
  }

  mainw->add_live_menu = lives_menu_item_new_with_mnemonic(_("_Add Webcam/TV card..."));

#if defined(HAVE_UNICAP) || defined(HAVE_YUV4MPEG)
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->add_live_menu);
  lives_widget_show(mainw->add_live_menu);

#ifndef HAVE_UNICAP
  if (capable->has_mplayer||capable->has_mplayer2) {
#endif

    submenu=lives_menu_new();
    lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->add_live_menu), submenu);
    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
      lives_widget_set_fg_color(submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
    }
    lives_widget_show(submenu);

#ifdef HAVE_UNICAP
    menuitem = lives_menu_item_new_with_mnemonic(_("Add _Unicap Device"));
    lives_container_add(LIVES_CONTAINER(submenu), menuitem);
    lives_widget_show(menuitem);
    lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_open_vdev_activate),
                         NULL);
#endif

#ifdef HAVE_YUV4MPEG
    if (capable->has_dvgrab) {
      menuitem = lives_menu_item_new_with_mnemonic(_("Add Live _Firewire Device"));
      lives_container_add(LIVES_CONTAINER(submenu), menuitem);
      lives_widget_show(menuitem);

      lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(on_live_fw_activate),
                           NULL);
    }

    menuitem = lives_menu_item_new_with_mnemonic(_("Add _TV Device"));
    lives_container_add(LIVES_CONTAINER(submenu), menuitem);
    lives_widget_show(menuitem);

    lives_signal_connect(LIVES_GUI_OBJECT(menuitem), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_live_tvcard_activate),
                         NULL);

#ifndef HAVE_UNICAP
  } // if (capable->has_mplayer)
#endif

#endif
#endif // defined HAVE_UNICAP || defined HAVE_YUV4MPEG

  mainw->recent_menu = lives_menu_item_new_with_mnemonic(_("_Recent Files..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->recent_menu);
  mainw->recent_submenu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->recent_menu), mainw->recent_submenu);

  memset(buff,0,1);

  // since we are still initialising, we need to check if we can read prefs
  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent1",buff,32768);
  }
  mainw->recent1 = lives_menu_item_new_with_label(buff);
  if (strlen(buff)) lives_widget_show(mainw->recent1);
  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent2",buff,32768);
  }
  mainw->recent2 = lives_menu_item_new_with_label(buff);
  if (strlen(buff)) lives_widget_show(mainw->recent2);

  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent3",buff,32768);
  }
  mainw->recent3 = lives_menu_item_new_with_label(buff);
  if (strlen(buff)) lives_widget_show(mainw->recent3);

  if (capable->smog_version_correct&&capable->can_write_to_tempdir) {
    get_pref_utf8("recent4",buff,32768);
  }
  mainw->recent4 = lives_menu_item_new_with_label(buff);
  if (strlen(buff)) lives_widget_show(mainw->recent4);

  lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent1);
  lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent2);
  lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent3);
  lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent4);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->recent_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->recent_submenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_widget_show(mainw->recent_submenu);

  if (capable->smog_version_correct&&prefs->show_recent) {
    lives_widget_show(mainw->recent_menu);
  }

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->vj_load_set = lives_menu_item_new_with_mnemonic(_("_Reload Clip Set..."));
  lives_widget_show(mainw->vj_load_set);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->vj_load_set);

  mainw->vj_save_set = lives_menu_item_new_with_mnemonic(_("Close/Sa_ve All Clips"));
  lives_widget_show(mainw->vj_save_set);
  lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->vj_save_set);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->save_as = lives_image_menu_item_new_from_stock(LIVES_STOCK_LABEL_SAVE, mainw->accel_group);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->save_as);
  lives_widget_set_sensitive(mainw->save_as, FALSE);
  set_menu_text(mainw->save_as,_("_Encode Clip As..."),TRUE);

  mainw->save_selection = lives_menu_item_new_with_mnemonic(_("Encode _Selection As..."));
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->save_selection);
  lives_widget_set_sensitive(mainw->save_selection, FALSE);

  lives_widget_show(mainw->save_as);
  lives_widget_show(mainw->save_selection);

  mainw->close = lives_menu_item_new_with_mnemonic(_("_Close This Clip"));
  lives_widget_add_accelerator(mainw->close, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_w, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_show(mainw->close);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->close);
  lives_widget_set_sensitive(mainw->close, FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->backup = lives_menu_item_new_with_mnemonic(_("_Backup Clip as .lv1..."));
  lives_widget_show(mainw->backup);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->backup);
  lives_widget_set_sensitive(mainw->backup, FALSE);

  lives_widget_add_accelerator(mainw->backup, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_b, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->restore = lives_menu_item_new_with_mnemonic(_("_Restore Clip from .lv1..."));
  lives_widget_show(mainw->restore);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->restore);

  lives_widget_add_accelerator(mainw->restore, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_r, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->sw_sound = lives_check_menu_item_new_with_mnemonic(_("Encode/Load/Backup _with Sound"));
  lives_widget_show(mainw->sw_sound);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->sw_sound);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sw_sound),TRUE);

  mainw->aload_subs = lives_check_menu_item_new_with_mnemonic(_("Auto load subtitles"));
  lives_widget_show(mainw->aload_subs);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->aload_subs);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->aload_subs),prefs->autoload_subs);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->clear_ds = lives_menu_item_new_with_mnemonic(_("Clean _up Diskspace"));
  lives_widget_show(mainw->clear_ds);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->clear_ds);

  mainw->quit = lives_image_menu_item_new_from_stock(LIVES_STOCK_LABEL_QUIT, mainw->accel_group);
  lives_widget_show(mainw->quit);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->quit);

  menuitem = lives_menu_item_new_with_mnemonic(_("_Edit"));
  lives_widget_show(menuitem);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  menuitem_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->undo = lives_image_menu_item_new_with_mnemonic(_("_Undo"));

  lives_widget_show(mainw->undo);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->undo);
  lives_widget_set_sensitive(mainw->undo, FALSE);


  lives_widget_add_accelerator(mainw->undo, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_u, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_UNDO, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->undo), image);
#endif

  mainw->redo = lives_image_menu_item_new_with_mnemonic(_("_Redo"));
  lives_widget_hide(mainw->redo);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->redo);
  lives_widget_set_sensitive(mainw->redo, FALSE);

  lives_widget_add_accelerator(mainw->redo, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_z, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REDO, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->redo), image);
#endif

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->mt_menu = lives_image_menu_item_new_with_mnemonic(_("_MULTITRACK mode"));
  lives_widget_show(mainw->mt_menu);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->mt_menu);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  lives_widget_add_accelerator(mainw->mt_menu, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_m, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->copy = lives_image_menu_item_new_with_mnemonic(_("_Copy Selection"));
  lives_widget_show(mainw->copy);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->copy);
  lives_widget_set_sensitive(mainw->copy, FALSE);

  lives_widget_add_accelerator(mainw->copy, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_c, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->cut = lives_image_menu_item_new_with_mnemonic(_("Cu_t Selection"));
  lives_widget_show(mainw->cut);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->cut);
  lives_widget_set_sensitive(mainw->cut, FALSE);

  lives_widget_add_accelerator(mainw->cut, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_t, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->insert = lives_image_menu_item_new_with_mnemonic(_("_Insert from Clipboard..."));
  lives_widget_show(mainw->insert);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->insert);
  lives_widget_set_sensitive(mainw->insert, FALSE);

  lives_widget_add_accelerator(mainw->insert, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_i, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_ADD, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->insert), image);
#endif

  mainw->paste_as_new = lives_image_menu_item_new_with_mnemonic(_("Paste as _New"));
  lives_widget_show(mainw->paste_as_new);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->paste_as_new);
  lives_widget_set_sensitive(mainw->paste_as_new, FALSE);

  lives_widget_add_accelerator(mainw->paste_as_new, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_n, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->merge = lives_menu_item_new_with_mnemonic(_("_Merge Clipboard with Selection..."));
  if (capable->has_composite&&capable->has_convert) {
    lives_widget_show(mainw->merge);
  }
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->merge);
  lives_widget_set_sensitive(mainw->merge, FALSE);

  mainw->xdelete = lives_image_menu_item_new_with_mnemonic(_("_Delete Selection"));
  lives_widget_show(mainw->xdelete);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->xdelete);
  lives_widget_set_sensitive(mainw->xdelete, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock("gtk-delete", LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->xdelete), image);
#endif

  lives_widget_add_accelerator(mainw->xdelete, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_d, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->ccpd_sound = lives_check_menu_item_new_with_mnemonic(_("Decouple _Video from Audio"));
  lives_widget_show(mainw->ccpd_sound);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->ccpd_sound);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->ccpd_sound),!mainw->ccpd_with_sound);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->select_submenu = lives_menu_item_new_with_mnemonic(_("_Select..."));
  lives_widget_show(mainw->select_submenu);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->select_submenu);
  lives_widget_set_sensitive(mainw->select_submenu,FALSE);

  select_submenu_menu=lives_menu_new();

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->select_submenu), select_submenu_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(select_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(select_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->select_all = lives_menu_item_new_with_mnemonic(_("Select _All Frames"));
  lives_widget_show(mainw->select_all);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_all);

  lives_widget_add_accelerator(mainw->select_all, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_a, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->select_start_only = lives_image_menu_item_new_with_mnemonic(_("_Start Frame Only"));
  lives_widget_show(mainw->select_start_only);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_start_only);

  lives_widget_add_accelerator(mainw->select_start_only, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_Home, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->select_end_only = lives_image_menu_item_new_with_mnemonic(_("_End Frame Only"));
  lives_widget_show(mainw->select_end_only);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_end_only);
  lives_widget_add_accelerator(mainw->select_end_only, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_End, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);


  separatormenuitem=lives_menu_add_separator(LIVES_MENU(select_submenu_menu));
  lives_widget_show(separatormenuitem);

  mainw->select_from_start = lives_image_menu_item_new_with_mnemonic(_("Select from _First Frame"));
  lives_widget_show(mainw->select_from_start);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_from_start);

  mainw->select_to_end = lives_image_menu_item_new_with_mnemonic(_("Select to _Last Frame"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_to_end);
  lives_widget_show(mainw->select_to_end);

  mainw->select_new = lives_image_menu_item_new_with_mnemonic(_("Select Last Insertion/_Merge"));
  lives_widget_show(mainw->select_new);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_new);

  mainw->select_last = lives_image_menu_item_new_with_mnemonic(_("Select Last _Effect"));
  lives_widget_show(mainw->select_last);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_last);

  mainw->select_invert = lives_image_menu_item_new_with_mnemonic(_("_Invert Selection"));
  lives_widget_show(mainw->select_invert);
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_invert);

  lives_widget_add_accelerator(mainw->select_invert, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_Slash, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->lock_selwidth = lives_check_menu_item_new_with_mnemonic(_("_Lock Selection Width"));
  lives_widget_show(mainw->lock_selwidth);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->lock_selwidth);
  lives_widget_set_sensitive(mainw->lock_selwidth,FALSE);

  menuitem = lives_menu_item_new_with_mnemonic(_("_Play"));
  lives_widget_show(menuitem);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  menuitem_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->playall = lives_image_menu_item_new_with_mnemonic(_("_Play All"));
  lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_p, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_show(mainw->playall);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->playall);
  lives_widget_set_sensitive(mainw->playall, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REFRESH, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playall), image);
#endif

  mainw->playsel = lives_image_menu_item_new_with_mnemonic(_("Pla_y Selection"));
  lives_widget_add_accelerator(mainw->playsel, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_y, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_show(mainw->playsel);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->playsel);
  lives_widget_set_sensitive(mainw->playsel, FALSE);

  mainw->playclip = lives_image_menu_item_new_with_mnemonic(_("Play _Clipboard"));
  lives_widget_show(mainw->playclip);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->playclip);
  lives_widget_set_sensitive(mainw->playclip, FALSE);

  lives_widget_add_accelerator(mainw->playclip, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_c, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playsel), image);
#endif

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playclip), image);
#endif

  mainw->stop = lives_image_menu_item_new_with_mnemonic(_("_Stop"));
  lives_widget_show(mainw->stop);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->stop);
  lives_widget_set_sensitive(mainw->stop, FALSE);
  lives_widget_add_accelerator(mainw->stop, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_q, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);


#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->stop), image);
#endif

  mainw->rewind = lives_image_menu_item_new_with_mnemonic(_("Re_wind"));
  lives_widget_show(mainw->rewind);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->rewind);
  lives_widget_set_sensitive(mainw->rewind, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, LIVES_ICON_SIZE_MENU);

  lives_widget_show(image);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->rewind), image);
#endif

  lives_widget_add_accelerator(mainw->rewind, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_w, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->record_perf = lives_check_menu_item_new_with_mnemonic("");

  disable_record();

  lives_widget_add_accelerator(mainw->record_perf, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_r, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  lives_widget_show(mainw->record_perf);

  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->record_perf);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->record_perf),FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->full_screen = lives_check_menu_item_new_with_mnemonic(_("_Full Screen"));
  lives_widget_show(mainw->full_screen);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->full_screen);


  lives_widget_add_accelerator(mainw->full_screen, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_f, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->dsize = lives_check_menu_item_new_with_mnemonic(_("_Double Size"));
  lives_widget_show(mainw->dsize);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->dsize);

  lives_widget_add_accelerator(mainw->dsize, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_d, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->sepwin = lives_check_menu_item_new_with_mnemonic(_("Play in _Separate Window"));
  lives_widget_show(mainw->sepwin);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->sepwin);

  lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_s, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);


  mainw->fade = lives_check_menu_item_new_with_mnemonic(_("_Blank Background"));
  lives_widget_add_accelerator(mainw->fade, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_b, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_show(mainw->fade);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->fade);

  mainw->loop_video = lives_check_menu_item_new_with_mnemonic(_("(Auto)_loop Video (to fit audio track)"));
  lives_widget_show(mainw->loop_video);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->loop_video);
  lives_widget_set_sensitive(mainw->loop_video, FALSE);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->loop_video),mainw->loop);
  lives_widget_add_accelerator(mainw->loop_video, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_l, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->loop_continue = lives_check_menu_item_new_with_mnemonic(_("L_oop Continuously"));
  lives_widget_show(mainw->loop_continue);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->loop_continue);
  lives_widget_set_sensitive(mainw->loop_continue, FALSE);

  lives_widget_add_accelerator(mainw->loop_continue, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_o, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->loop_ping_pong = lives_check_menu_item_new_with_mnemonic(_("Pin_g Pong Loops"));
  lives_widget_show(mainw->loop_ping_pong);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->loop_ping_pong);

  lives_widget_add_accelerator(mainw->loop_ping_pong, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_g, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  mainw->mute_audio = lives_check_menu_item_new_with_mnemonic(_("_Mute"));
  lives_widget_show(mainw->mute_audio);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->mute_audio);
  lives_widget_set_sensitive(mainw->mute_audio, FALSE);

  lives_widget_add_accelerator(mainw->mute_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_z, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->sticky = lives_check_menu_item_new_with_mnemonic(_("Separate Window 'S_ticky' Mode"));
  lives_widget_show(mainw->sticky);

  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->sticky);
  if (capable->smog_version_correct&&prefs->sepwin_type==SEPWIN_TYPE_STICKY) {
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sticky),TRUE);
  }

  mainw->showfct = lives_check_menu_item_new_with_mnemonic(_("S_how Frame Counter"));
  lives_widget_show(mainw->showfct);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->showfct);

  lives_widget_add_accelerator(mainw->showfct, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_h, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->showfct),capable->smog_version_correct&&prefs->show_framecount);

  mainw->showsubs = lives_check_menu_item_new_with_mnemonic(_("Show Subtitles"));
  lives_widget_show(mainw->showsubs);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->showsubs);

  lives_widget_add_accelerator(mainw->showsubs, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_v, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);

  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->showsubs),prefs->show_subtitles);

  mainw->letter = lives_check_menu_item_new_with_mnemonic(_("Letterbox mode"));
  lives_widget_show(mainw->letter);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->letter);

  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->letter),prefs->letterbox);

  effects = lives_menu_item_new_with_mnemonic(_("Effect_s"));
  lives_widget_show(effects);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), effects);
  lives_widget_set_tooltip_text(effects,(_("Effects are applied to the current selection.")));

  // the dynamic effects menu
  mainw->effects_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(effects), mainw->effects_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->effects_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->effects_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->custom_effects_menu=NULL;

  mainw->run_test_rfx_submenu=lives_menu_item_new_with_mnemonic(_("_Run Test Rendered Effect/Tool/Generator..."));
  mainw->run_test_rfx_menu=NULL;

  mainw->num_rendered_effects_builtin=mainw->num_rendered_effects_custom=mainw->num_rendered_effects_test=0;

  tools = lives_menu_item_new_with_mnemonic(_("_Tools"));
  lives_widget_show(tools);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), tools);
  lives_widget_set_tooltip_text(tools,(_("Tools are applied to complete clips.")));

  mainw->tools_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(tools), mainw->tools_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->tools_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->tools_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->rev_clipboard = lives_menu_item_new_with_mnemonic(_("_Reverse Clipboard"));
  lives_widget_show(mainw->rev_clipboard);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->rev_clipboard);
  lives_widget_set_sensitive(mainw->rev_clipboard, FALSE);

  lives_widget_add_accelerator(mainw->rev_clipboard, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_x, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

  mainw->change_speed = lives_menu_item_new_with_mnemonic(_("_Change Playback/Save Speed..."));
  lives_widget_show(mainw->change_speed);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->change_speed);
  lives_widget_set_sensitive(mainw->change_speed, FALSE);

  mainw->resample_video = lives_menu_item_new_with_mnemonic(_("Resample _Video to New Frame Rate..."));
  lives_widget_show(mainw->resample_video);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->resample_video);
  lives_widget_set_sensitive(mainw->resample_video, FALSE);

  mainw->utilities_menu=NULL;
  mainw->utilities_submenu = lives_menu_item_new_with_mnemonic(_("_Utilities"));
  lives_widget_show(mainw->utilities_submenu);

  mainw->custom_utilities_menu=NULL;

  mainw->custom_tools_submenu = lives_menu_item_new_with_mnemonic(_("Custom _Tools"));

  mainw->custom_tools_separator = lives_menu_item_new();
  lives_widget_set_sensitive(mainw->custom_tools_separator, FALSE);

  mainw->gens_menu=NULL;
  mainw->gens_submenu = lives_menu_item_new_with_mnemonic(_("_Generate"));
  lives_widget_show(mainw->gens_submenu);

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

  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->utilities_submenu);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->custom_tools_separator);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->custom_tools_submenu);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->gens_submenu);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));
  lives_widget_show(separatormenuitem);

  mainw->load_subs = lives_menu_item_new_with_mnemonic(_("Load _Subtitles from File..."));
  lives_widget_show(mainw->load_subs);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->load_subs);
  lives_widget_set_sensitive(mainw->load_subs, FALSE);

  mainw->erase_subs = lives_menu_item_new_with_mnemonic(_("Erase subtitles"));
  lives_widget_show(mainw->erase_subs);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->erase_subs);
  lives_widget_set_sensitive(mainw->erase_subs, FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));
  lives_widget_show(separatormenuitem);

  mainw->capture = lives_menu_item_new_with_mnemonic(_("Capture _External Window... "));
  lives_widget_show(mainw->capture);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->capture);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));
  lives_widget_show(separatormenuitem);

  mainw->preferences = lives_image_menu_item_new_with_mnemonic(_("_Preferences..."));
  lives_widget_show(mainw->preferences);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->preferences);
  lives_widget_add_accelerator(mainw->preferences, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_p, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_PREFERENCES, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->preferences), image);
#endif

  audio = lives_menu_item_new_with_mnemonic(_("_Audio"));
  lives_widget_show(audio);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), audio);

  audio_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(audio), audio_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(audio_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(audio_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->load_audio = lives_menu_item_new_with_mnemonic(_("Load _New Audio for Clip..."));

  lives_widget_show(mainw->load_audio);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->load_audio);
  lives_widget_set_sensitive(mainw->load_audio, FALSE);

  mainw->load_cdtrack = lives_menu_item_new_with_mnemonic(_("Load _CD Track..."));
  mainw->eject_cd = lives_menu_item_new_with_mnemonic(_("E_ject CD"));
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->load_cdtrack);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->eject_cd);

  if (capable->smog_version_correct) {
    if (!(capable->has_cdda2wav&&strlen(prefs->cdplay_device))) {
      lives_widget_set_sensitive(mainw->load_cdtrack, FALSE);
      lives_widget_set_sensitive(mainw->eject_cd, FALSE);
    }
  }

  lives_widget_show(mainw->eject_cd);
  lives_widget_show(mainw->load_cdtrack);

  mainw->recaudio_submenu = lives_menu_item_new_with_mnemonic(_("Record E_xternal Audio..."));
  if ((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||(prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))
    lives_widget_show(mainw->recaudio_submenu);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->recaudio_submenu);

  submenu_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->recaudio_submenu), submenu_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(mainw->recaudio_submenu);

  mainw->recaudio_clip = lives_menu_item_new_with_mnemonic(_("to New _Clip..."));
  lives_widget_show(mainw->recaudio_clip);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mainw->recaudio_clip);

  mainw->recaudio_sel = lives_menu_item_new_with_mnemonic(_("to _Selection"));
  lives_widget_show(mainw->recaudio_sel);
  lives_container_add(LIVES_CONTAINER(submenu_menu), mainw->recaudio_sel);
  lives_widget_set_sensitive(mainw->recaudio_sel,FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(audio_menu));
  lives_widget_show(separatormenuitem);

  mainw->fade_aud_in = lives_menu_item_new_with_mnemonic(_("Fade Audio _In..."));
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->fade_aud_in);
  lives_widget_show(mainw->fade_aud_in);

  mainw->fade_aud_out = lives_menu_item_new_with_mnemonic(_("Fade Audio _Out..."));
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->fade_aud_out);
  lives_widget_show(mainw->fade_aud_out);

  lives_widget_set_sensitive(mainw->fade_aud_in, FALSE);
  lives_widget_set_sensitive(mainw->fade_aud_out, FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(audio_menu));
  lives_widget_show(separatormenuitem);

  mainw->export_submenu = lives_menu_item_new_with_mnemonic(_("_Export Audio..."));
  lives_widget_show(mainw->export_submenu);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->export_submenu);
  lives_widget_set_sensitive(mainw->export_submenu,FALSE);

  export_submenu_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->export_submenu), export_submenu_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(export_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(export_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(mainw->export_submenu);

  mainw->export_selaudio = lives_menu_item_new_with_mnemonic(_("Export _Selected Audio..."));
  lives_widget_show(mainw->export_selaudio);
  lives_container_add(LIVES_CONTAINER(export_submenu_menu), mainw->export_selaudio);

  mainw->export_allaudio = lives_menu_item_new_with_mnemonic(_("Export _All Audio..."));
  lives_widget_show(mainw->export_allaudio);
  lives_container_add(LIVES_CONTAINER(export_submenu_menu), mainw->export_allaudio);

  mainw->append_audio = lives_menu_item_new_with_mnemonic(_("_Append Audio..."));
  lives_widget_show(mainw->append_audio);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->append_audio);
  lives_widget_set_sensitive(mainw->append_audio, FALSE);

  mainw->trim_submenu = lives_menu_item_new_with_mnemonic(_("_Trim/Pad Audio..."));
  lives_widget_show(mainw->trim_submenu);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->trim_submenu);
  lives_widget_set_sensitive(mainw->trim_submenu,FALSE);

  trimaudio_submenu_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->trim_submenu), trimaudio_submenu_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(trimaudio_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(trimaudio_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_widget_show(mainw->trim_submenu);
  mainw->trim_audio = lives_menu_item_new_with_mnemonic(_("Trim/Pad Audio to _Selection"));
  lives_widget_show(mainw->trim_audio);
  lives_container_add(LIVES_CONTAINER(trimaudio_submenu_menu), mainw->trim_audio);
  lives_widget_set_sensitive(mainw->trim_audio, FALSE);

  mainw->trim_to_pstart = lives_menu_item_new_with_mnemonic(_("Trim/Pad Audio from Beginning to _Play Start"));
  lives_widget_show(mainw->trim_to_pstart);
  lives_container_add(LIVES_CONTAINER(trimaudio_submenu_menu), mainw->trim_to_pstart);
  lives_widget_set_sensitive(mainw->trim_to_pstart, FALSE);

  mainw->delaudio_submenu = lives_menu_item_new_with_mnemonic(_("_Delete Audio..."));
  lives_widget_show(mainw->delaudio_submenu);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->delaudio_submenu);
  lives_widget_set_sensitive(mainw->delaudio_submenu,FALSE);

  delaudio_submenu_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->delaudio_submenu), delaudio_submenu_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(delaudio_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(delaudio_submenu_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(mainw->delaudio_submenu);

  mainw->delsel_audio = lives_menu_item_new_with_mnemonic(_("Delete _Selected Audio"));
  lives_widget_show(mainw->delsel_audio);
  lives_container_add(LIVES_CONTAINER(delaudio_submenu_menu), mainw->delsel_audio);

  mainw->delall_audio = lives_menu_item_new_with_mnemonic(_("Delete _All Audio"));
  lives_widget_show(mainw->delall_audio);
  lives_container_add(LIVES_CONTAINER(delaudio_submenu_menu), mainw->delall_audio);

  mainw->ins_silence = lives_menu_item_new_with_mnemonic(_("Insert _Silence in Selection"));
  lives_widget_show(mainw->ins_silence);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->ins_silence);
  lives_widget_set_sensitive(mainw->ins_silence, FALSE);

  mainw->resample_audio = lives_menu_item_new_with_mnemonic(_("_Resample Audio..."));
  lives_widget_show(mainw->resample_audio);
  lives_container_add(LIVES_CONTAINER(audio_menu), mainw->resample_audio);
  lives_widget_set_sensitive(mainw->resample_audio, FALSE);

  info = lives_menu_item_new_with_mnemonic(_("_Info"));
  lives_widget_show(info);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), info);

  info_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(info), info_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(info_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(info_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->show_file_info = lives_image_menu_item_new_with_mnemonic(_("Show Clip _Info"));
  lives_widget_add_accelerator(mainw->show_file_info, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_i, (LiVESXModifierType)0,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_show(mainw->show_file_info);
  lives_container_add(LIVES_CONTAINER(info_menu), mainw->show_file_info);
  lives_widget_set_sensitive(mainw->show_file_info, FALSE);

  mainw->show_file_comments = lives_image_menu_item_new_with_mnemonic(_("Show/_Edit File Comments"));
  lives_widget_show(mainw->show_file_comments);
  lives_container_add(LIVES_CONTAINER(info_menu), mainw->show_file_comments);
  lives_widget_set_sensitive(mainw->show_file_comments, FALSE);

  mainw->show_clipboard_info = lives_image_menu_item_new_with_mnemonic(_("Show _Clipboard Info"));
  lives_widget_show(mainw->show_clipboard_info);
  lives_container_add(LIVES_CONTAINER(info_menu), mainw->show_clipboard_info);
  lives_widget_set_sensitive(mainw->show_clipboard_info, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_INFO, LIVES_ICON_SIZE_MENU);
  lives_widget_show(image);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->show_file_info), image);
#endif

  mainw->show_messages = lives_image_menu_item_new_with_mnemonic(_("Show _Messages"));
  lives_widget_show(mainw->show_messages);
  lives_container_add(LIVES_CONTAINER(info_menu), mainw->show_messages);

  mainw->show_layout_errors = lives_image_menu_item_new_with_mnemonic(_("Show _Layout Errors"));
  lives_widget_show(mainw->show_layout_errors);
  lives_container_add(LIVES_CONTAINER(info_menu), mainw->show_layout_errors);
  lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);

  win = lives_menu_item_new_with_mnemonic(_("_Clips"));
  lives_widget_show(win);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), win);

  mainw->clipsmenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(win), mainw->clipsmenu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->clipsmenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->clipsmenu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->rename = lives_image_menu_item_new_with_mnemonic(_("_Rename Current Clip in Menu..."));
  lives_widget_show(mainw->rename);
  lives_container_add(LIVES_CONTAINER(mainw->clipsmenu), mainw->rename);
  lives_widget_set_sensitive(mainw->rename, FALSE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(mainw->clipsmenu));
  lives_widget_show(separatormenuitem);

  menuitemsep = lives_menu_item_new_with_label("|");
  lives_widget_show(menuitemsep);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitemsep);
  lives_widget_set_sensitive(menuitemsep,FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menuitemsep, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars);
    lives_widget_set_fg_color(menuitemsep, LIVES_WIDGET_STATE_INSENSITIVE, &palette->menu_and_bars_fore);
  }

  advanced = lives_menu_item_new_with_mnemonic(_("A_dvanced"));
  lives_widget_show(advanced);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), advanced);

  advanced_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(advanced), advanced_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(advanced_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(advanced_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(advanced_menu);

  rfx_submenu = lives_menu_item_new_with_mnemonic(_("_RFX Effects/Tools/Utilities"));
  lives_widget_show(rfx_submenu);
  lives_container_add(LIVES_CONTAINER(advanced_menu), rfx_submenu);

  rfx_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(rfx_submenu), rfx_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(rfx_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(rfx_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(rfx_menu);

  new_test_rfx=lives_menu_item_new_with_mnemonic(_("_New Test RFX Script..."));
  lives_widget_show(new_test_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), new_test_rfx);

  copy_rfx=lives_menu_item_new_with_mnemonic(_("_Copy RFX Script to Test..."));
  lives_widget_show(copy_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), copy_rfx);

  mainw->edit_test_rfx=lives_menu_item_new_with_mnemonic(_("_Edit Test RFX Script..."));
  lives_widget_show(mainw->edit_test_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->edit_test_rfx);

  mainw->rename_test_rfx=lives_menu_item_new_with_mnemonic(_("Rena_me Test RFX Script..."));
  lives_widget_show(mainw->rename_test_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->rename_test_rfx);

  mainw->delete_test_rfx=lives_menu_item_new_with_mnemonic(_("_Delete Test RFX Script..."));
  lives_widget_show(mainw->delete_test_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->delete_test_rfx);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(rfx_menu));
  lives_widget_show(separatormenuitem);

  lives_widget_show(mainw->run_test_rfx_submenu);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->run_test_rfx_submenu);

  mainw->promote_test_rfx=lives_menu_item_new_with_mnemonic(_("_Promote Test Rendered Effect/Tool/Generator..."));
  lives_widget_show(mainw->promote_test_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->promote_test_rfx);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(rfx_menu));
  lives_widget_show(separatormenuitem);

  import_custom_rfx=lives_menu_item_new_with_mnemonic(_("_Import Custom RFX script..."));
  lives_widget_show(import_custom_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), import_custom_rfx);

  mainw->export_custom_rfx=lives_menu_item_new_with_mnemonic(_("E_xport Custom RFX script..."));
  lives_widget_show(mainw->export_custom_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->export_custom_rfx);

  mainw->delete_custom_rfx=lives_menu_item_new_with_mnemonic(_("De_lete Custom RFX Script..."));
  lives_widget_show(mainw->delete_custom_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->delete_custom_rfx);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(rfx_menu));
  lives_widget_show(separatormenuitem);

  rebuild_rfx=lives_menu_item_new_with_mnemonic(_("Re_build all RFX plugins"));
  lives_widget_show(rebuild_rfx);
  lives_container_add(LIVES_CONTAINER(rfx_menu), rebuild_rfx);


  if (mainw->num_rendered_effects_custom>0) {
    lives_widget_set_sensitive(mainw->delete_custom_rfx,TRUE);
    lives_widget_set_sensitive(mainw->export_custom_rfx,TRUE);
  } else {
    lives_widget_set_sensitive(mainw->delete_custom_rfx,FALSE);
    lives_widget_set_sensitive(mainw->export_custom_rfx,FALSE);
  }

  if (mainw->num_rendered_effects_test>0) {
    lives_widget_set_sensitive(mainw->run_test_rfx_submenu,TRUE);
    lives_widget_set_sensitive(mainw->promote_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->delete_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->rename_test_rfx,TRUE);
    lives_widget_set_sensitive(mainw->edit_test_rfx,TRUE);
  } else {
    lives_widget_set_sensitive(mainw->run_test_rfx_submenu,FALSE);
    lives_widget_set_sensitive(mainw->promote_test_rfx,FALSE);
    lives_widget_set_sensitive(mainw->delete_test_rfx,FALSE);
    lives_widget_set_sensitive(mainw->rename_test_rfx,FALSE);
    lives_widget_set_sensitive(mainw->edit_test_rfx,FALSE);
  }

  mainw->open_lives2lives = lives_menu_item_new_with_mnemonic(_("Receive _LiVES stream from..."));

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(advanced_menu));
  lives_widget_show(separatormenuitem);

  mainw->send_lives2lives = lives_menu_item_new_with_mnemonic(_("_Send LiVES stream to..."));

  lives_widget_show(mainw->send_lives2lives);
  lives_widget_show(mainw->open_lives2lives);

  lives_container_add(LIVES_CONTAINER(advanced_menu), mainw->send_lives2lives);
  lives_container_add(LIVES_CONTAINER(advanced_menu), mainw->open_lives2lives);

  if (capable->smog_version_correct) {
    mainw->open_yuv4m = lives_menu_item_new_with_mnemonic((tmp=lives_strdup_printf(_("Open _yuv4mpeg stream on %sstream.yuv..."),
                        prefs->tmpdir)));
    lives_free(tmp);
#ifdef HAVE_YUV4MPEG
    separatormenuitem=lives_menu_add_separator(LIVES_MENU(advanced_menu));
    lives_widget_show(separatormenuitem);

    lives_widget_show(mainw->open_yuv4m);
    lives_container_add(LIVES_CONTAINER(advanced_menu), mainw->open_yuv4m);

    // TODO - apply a deinterlace filter to yuv4mpeg frames
    /*    mainw->yuv4m_deint = lives_check_menu_item_new_with_mnemonic (_("_Deinterlace yuv4mpeg frames"));
    lives_widget_show (mainw->yuv4m_deint);
    lives_container_add (LIVES_CONTAINER (advance_menu), mainw->yuv4m_deint);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->yu4m_deint),TRUE);*/
#endif
  }


  separatormenuitem=lives_menu_add_separator(LIVES_MENU(advanced_menu));
  lives_widget_show(separatormenuitem);

  mainw->import_proj = lives_menu_item_new_with_mnemonic(_("_Import Project (.lv2)..."));
  lives_container_add(LIVES_CONTAINER(advanced_menu), mainw->import_proj);
  lives_widget_show(mainw->import_proj);

  mainw->export_proj = lives_menu_item_new_with_mnemonic(_("E_xport Project (.lv2)..."));
  lives_container_add(LIVES_CONTAINER(advanced_menu), mainw->export_proj);
  lives_widget_show(mainw->export_proj);
  lives_widget_set_sensitive(mainw->export_proj, FALSE);

  mainw->vj_menu = lives_menu_item_new_with_mnemonic(_("_VJ"));
  lives_widget_show(mainw->vj_menu);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), mainw->vj_menu);

  vj_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->vj_menu), vj_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(vj_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(vj_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  assign_rte_keys = lives_menu_item_new_with_mnemonic(_("Real Time _Effect Mapping"));
  lives_widget_show(assign_rte_keys);
  lives_container_add(LIVES_CONTAINER(vj_menu), assign_rte_keys);
  lives_widget_add_accelerator(assign_rte_keys, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_v, LIVES_CONTROL_MASK,
                               LIVES_ACCEL_VISIBLE);
  lives_widget_set_tooltip_text(assign_rte_keys,(_("Bind real time effects to ctrl-number keys.")));

  mainw->rte_defs_menu=lives_menu_item_new_with_mnemonic(_("Set Real Time Effect _Defaults"));
  lives_container_add(LIVES_CONTAINER(vj_menu), mainw->rte_defs_menu);
  lives_widget_set_tooltip_text(mainw->rte_defs_menu,(_("Set default parameter values for real time effects.")));

  mainw->rte_defs=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->rte_defs_menu), mainw->rte_defs);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->rte_defs, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->rte_defs, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_widget_show(mainw->rte_defs_menu);
  lives_widget_show(mainw->rte_defs);

  mainw->save_rte_defs=lives_menu_item_new_with_mnemonic(_("Save Real Time Effect _Defaults"));
  lives_container_add(LIVES_CONTAINER(vj_menu), mainw->save_rte_defs);
  lives_widget_show(mainw->save_rte_defs);
  lives_widget_set_tooltip_text(mainw->save_rte_defs,(_("Save real time effect defaults so they will be restored each time you use LiVES.")));


  separatormenuitem=lives_menu_add_separator(LIVES_MENU(vj_menu));
  lives_widget_show(separatormenuitem);

  mainw->vj_reset=lives_menu_item_new_with_mnemonic(_("_Reset all playback speeds and positions"));
  lives_container_add(LIVES_CONTAINER(vj_menu), mainw->vj_reset);
  lives_widget_show(mainw->vj_reset);
  lives_widget_set_tooltip_text(mainw->vj_reset,(_("Reset all playback positions to frame 1, and reset all playback frame rates.")));

  midi_submenu = lives_menu_item_new_with_mnemonic(_("_MIDI/joystick interface"));

#ifdef ENABLE_OSC
  lives_widget_show(midi_submenu);
  lives_container_add(LIVES_CONTAINER(vj_menu), midi_submenu);
#endif

  midi_menu=lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(midi_submenu), midi_menu);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(midi_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(midi_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }
  lives_widget_show(midi_menu);

  midi_learn = lives_menu_item_new_with_mnemonic(_("_MIDI/joystick learner..."));

  lives_widget_show(midi_learn);
  lives_container_add(LIVES_CONTAINER(midi_menu), midi_learn);

  midi_save = lives_menu_item_new_with_mnemonic(_("_Save device mapping..."));

  lives_widget_show(midi_save);
  lives_container_add(LIVES_CONTAINER(midi_menu), midi_save);


  midi_load = lives_menu_item_new_with_mnemonic(_("_Load device mapping..."));

  lives_widget_show(midi_load);
  lives_container_add(LIVES_CONTAINER(midi_menu), midi_load);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(vj_menu));
  lives_widget_show(separatormenuitem);

  mainw->vj_show_keys = lives_menu_item_new_with_mnemonic(_("Show VJ _Keys"));
  lives_widget_show(mainw->vj_show_keys);
  lives_container_add(LIVES_CONTAINER(vj_menu), mainw->vj_show_keys);

  mainw->toys = lives_menu_item_new_with_mnemonic(_("To_ys"));
  lives_widget_show(mainw->toys);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), mainw->toys);

  toys_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->toys), toys_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(toys_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(toys_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  mainw->toy_none = lives_check_menu_item_new_with_mnemonic(_("_None"));
  lives_widget_show(mainw->toy_none);
  lives_container_add(LIVES_CONTAINER(toys_menu), mainw->toy_none);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_none),TRUE);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(toys_menu));
  lives_widget_show(separatormenuitem);

  mainw->toy_autolives = lives_check_menu_item_new_with_mnemonic(_("_Autolives"));
  lives_widget_show(mainw->toy_autolives);
  lives_container_add(LIVES_CONTAINER(toys_menu), mainw->toy_autolives);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_autolives),FALSE);

  mainw->toy_random_frames = lives_check_menu_item_new_with_mnemonic(_("_Mad Frames"));
  lives_widget_show(mainw->toy_random_frames);
  lives_container_add(LIVES_CONTAINER(toys_menu), mainw->toy_random_frames);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->toy_random_frames),FALSE);

  mainw->toy_tv = lives_check_menu_item_new_with_mnemonic(_("_LiVES TV (broadband)"));

  lives_container_add(LIVES_CONTAINER(toys_menu), mainw->toy_tv);

#ifdef LIVES_TV_CHANNEL1
  lives_widget_show(mainw->toy_tv);
#else
  lives_widget_hide(mainw->toy_tv);
# endif

  menuitem = lives_menu_item_new_with_mnemonic(_("_Help"));
  lives_widget_show(menuitem);
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  menuitem_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), menuitem_menu);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(menuitem_menu, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  show_manual = lives_menu_item_new_with_mnemonic(_("_Manual (opens in browser)"));
  lives_widget_show(show_manual);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), show_manual);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  donate = lives_menu_item_new_with_mnemonic(_("_Donate to the project !"));
  lives_widget_show(donate);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), donate);

  email_author = lives_menu_item_new_with_mnemonic(_("_Email the author"));
  lives_widget_show(email_author);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), email_author);

  report_bug = lives_menu_item_new_with_mnemonic(_("Report a _bug"));
  lives_widget_show(report_bug);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), report_bug);

  suggest_feature = lives_menu_item_new_with_mnemonic(_("Suggest a _feature"));
  lives_widget_show(suggest_feature);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), suggest_feature);

  help_translate = lives_menu_item_new_with_mnemonic(_("Assist with _translating"));
  lives_widget_show(help_translate);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), help_translate);

  separatormenuitem=lives_menu_add_separator(LIVES_MENU(menuitem_menu));
  lives_widget_show(separatormenuitem);

  mainw->troubleshoot=lives_menu_item_new_with_mnemonic(_("_Troubleshoot"));
  lives_widget_show(mainw->troubleshoot);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), mainw->troubleshoot);

  about = lives_menu_item_new_with_mnemonic(_("_About"));
  lives_widget_show(about);
  lives_container_add(LIVES_CONTAINER(menuitem_menu), about);

  mainw->btoolbar=lives_toolbar_new();
  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mainw->btoolbar),TRUE);
  lives_box_pack_start(LIVES_BOX(mainw->menu_hbox), mainw->btoolbar, TRUE, TRUE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->btoolbar, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
    lives_widget_set_fg_color(mainw->btoolbar, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars_fore);
  }

  lives_toolbar_set_style(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(mainw->btoolbar),LIVES_ICON_SIZE_MENU);

  if (capable->smog_version_correct) {
    fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
    lives_snprintf(buff,PATH_MAX,"%s",fnamex);
    lives_free(fnamex);
    tmp_toolbar_icon=lives_image_new_from_file(buff);
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
      lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }

    mainw->m_sepwinbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_sepwinbutton),0);
    lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Show the play window (s)"));

    tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

    mainw->m_rewindbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_rewindbutton),-1);
    lives_widget_set_tooltip_text(mainw->m_rewindbutton,_("Rewind to start (w)"));

    lives_widget_set_sensitive(mainw->m_rewindbutton,FALSE);

    tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

    mainw->m_playbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_playbutton),-1);
    lives_widget_set_tooltip_text(mainw->m_playbutton,_("Play all (p)"));
    lives_widget_set_sensitive(mainw->m_playbutton,FALSE);


    tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

    mainw->m_stopbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_stopbutton),-1);
    lives_widget_set_tooltip_text(mainw->m_stopbutton,_("Stop playback (q)"));

    lives_widget_set_sensitive(mainw->m_stopbutton,FALSE);

    fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"playsel.png",NULL);
    lives_snprintf(buff,PATH_MAX,"%s",fnamex);
    lives_free(fnamex);
    tmp_toolbar_icon=lives_image_new_from_file(buff);

    mainw->m_playselbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_playselbutton),-1);
    lives_widget_set_tooltip_text(mainw->m_playselbutton,_("Play selection (y)"));

    lives_widget_set_sensitive(mainw->m_playselbutton,FALSE);


    fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
    lives_snprintf(buff,PATH_MAX,"%s",fnamex);
    lives_free(fnamex);
    tmp_toolbar_icon=lives_image_new_from_file(buff);
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
      lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }

    mainw->m_loopbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_loopbutton),-1);
    lives_widget_set_tooltip_text(mainw->m_loopbutton,_("Switch continuous looping on (o)"));

    fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
    lives_snprintf(buff,PATH_MAX,"%s",fnamex);
    lives_free(fnamex);
    tmp_toolbar_icon=lives_image_new_from_file(buff);
    if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
      pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
      lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
    }

    mainw->m_mutebutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->m_mutebutton),-1);
    lives_widget_set_tooltip_text(mainw->m_mutebutton,_("Mute the audio (z)"));

    lives_widget_show_all(mainw->btoolbar);

  } else {
    mainw->m_sepwinbutton = lives_menu_item_new();
    mainw->m_rewindbutton = lives_menu_item_new();
    mainw->m_playbutton = lives_menu_item_new();
    mainw->m_stopbutton = lives_menu_item_new();
    mainw->m_playselbutton = lives_menu_item_new();
    mainw->m_loopbutton = lives_menu_item_new();
    mainw->m_mutebutton = lives_menu_item_new();
  }

  adj=lives_adjustment_new(mainw->volume,0.,1.,0.01,0.1,0.);

  mainw->volume_scale=lives_volume_button_new(LIVES_ORIENTATION_HORIZONTAL,adj,mainw->volume);

  mainw->vol_label=NULL;

  if (LIVES_IS_RANGE(mainw->volume_scale)) {
    if (capable->smog_version_correct) {
      mainw->vol_label=LIVES_WIDGET(lives_tool_item_new());
      label=lives_label_new(_("Volume"));
      lives_container_add(LIVES_CONTAINER(mainw->vol_label),label);
      lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->vol_label),-1);
      lives_widget_show(mainw->vol_label);
    }
  } else lives_object_unref(adj);

  lives_widget_show(mainw->volume_scale);

  mainw->vol_toolitem=LIVES_WIDGET(lives_tool_item_new());

#ifdef GUI_GTK
  gtk_tool_item_set_homogeneous(LIVES_TOOL_ITEM(mainw->vol_toolitem),FALSE);
  gtk_tool_item_set_expand(LIVES_TOOL_ITEM(mainw->vol_toolitem),TRUE);
#endif

  if ((prefs->audio_player==AUD_PLAYER_JACK&&capable->has_jackd)||
      (prefs->audio_player==AUD_PLAYER_PULSE&&capable->has_pulse_audio))
    lives_widget_show(mainw->vol_toolitem);

  lives_container_add(LIVES_CONTAINER(mainw->vol_toolitem),mainw->volume_scale);
  if (capable->smog_version_correct) {
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar),LIVES_TOOL_ITEM(mainw->vol_toolitem),-1);
  }
  lives_widget_set_tooltip_text(mainw->vol_toolitem,_("Audio volume (1.00)"));

  lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->volume_scale), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_volume_slider_value_changed),
                             NULL);

  mainw->play_window=NULL;

  mainw->tb_hbox=lives_hbox_new(FALSE, 0);
  mainw->toolbar = lives_toolbar_new();
  lives_widget_set_bg_color(mainw->tb_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->toolbar, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mainw->toolbar),FALSE);

  lives_box_pack_start(LIVES_BOX(mainw->vbox1), mainw->tb_hbox, FALSE, FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->tb_hbox), mainw->toolbar, TRUE, TRUE, 0);

  lives_toolbar_set_style(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(mainw->toolbar),LIVES_ICON_SIZE_SMALL_TOOLBAR);
  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_stopbutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_stopbutton),0);
  lives_widget_set_tooltip_text(mainw->t_stopbutton,_("Stop playback (q)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_UNDO, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_bckground=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_bckground),1);
  lives_widget_set_tooltip_text(mainw->t_bckground,_("Unblank background (b)"));

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"sepwin.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  tmp_toolbar_icon=lives_image_new_from_file(buff);
  if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)&&!mainw->sep_win) {
    pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
    lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->t_sepwin=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_sepwin),2);
  lives_widget_set_tooltip_text(mainw->t_sepwin,_("Play in separate window (s)"));

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"zoom-in.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  tmp_toolbar_icon=lives_image_new_from_file(buff);
  if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)&&!mainw->double_size) {
    pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
    lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }
  mainw->t_double=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_widget_set_tooltip_text(mainw->t_double,_("Double size (d)"));

  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_double),3);

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"fullscreen.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  tmp_toolbar_icon=lives_image_new_from_file(buff);
  if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)) {
    pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(tmp_toolbar_icon));
    lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->t_fullscreen=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_fullscreen),4);
  lives_widget_set_tooltip_text(mainw->t_fullscreen,_("Fullscreen playback (f)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_REMOVE, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));


  mainw->t_slower=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_slower),5);
  lives_widget_set_tooltip_text(mainw->t_slower,_("Play slower (ctrl-down)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_ADD, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_faster=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_faster),6);
  lives_widget_set_tooltip_text(mainw->t_faster,_("Play faster (ctrl-up)"));


  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_GO_BACK, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_back=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_back),7);
  lives_widget_set_tooltip_text(mainw->t_back,_("Skip back (ctrl-left)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_GO_FORWARD, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_forward=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_forward),8);
  lives_widget_set_tooltip_text(mainw->t_forward,_("Skip forward (ctrl-right)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_DIALOG_INFO, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_infobutton=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_infobutton),9);
  lives_widget_set_tooltip_text(mainw->t_infobutton,_("Show clip info (i)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_CLOSE, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_hide=LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon),""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar),LIVES_TOOL_ITEM(mainw->t_hide),10);
  lives_widget_set_tooltip_text(mainw->t_hide,_("Hide this toolbar"));

  t_label=lives_label_new(_("Press \"s\" to toggle separate play window for improved performance, \"q\" to stop."));
  lives_widget_set_fg_color(t_label, LIVES_WIDGET_STATE_NORMAL, &palette->white);
  lives_box_pack_start(LIVES_BOX(mainw->tb_hbox), t_label, FALSE, FALSE, 0);

  lives_widget_show_all(mainw->tb_hbox);
  lives_widget_hide(mainw->tb_hbox);

  vbox4 = lives_vbox_new(FALSE, 0);
  lives_widget_show(vbox4);

  mainw->eventbox = lives_event_box_new();
  lives_widget_show(mainw->eventbox);
  lives_box_pack_start(LIVES_BOX(mainw->vbox1), mainw->eventbox, TRUE, TRUE, 0);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox), vbox4);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_bg_color(vbox4, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_bg_color(mainw->vbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  lives_widget_set_events(mainw->eventbox, LIVES_SCROLL_MASK);

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox), LIVES_WIDGET_SCROLL_EVENT,
                       LIVES_GUI_CALLBACK(on_mouse_scroll),
                       NULL);

  mainw->framebar = lives_hbox_new(FALSE, 0);
  lives_widget_show(mainw->framebar);
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->framebar, FALSE, FALSE, 0);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->framebar), 2*widget_opts.scale);

  /* TRANSLATORS: please keep the translated string the same length */
  mainw->vps_label = lives_standard_label_new(_("     Video playback speed (frames per second)        "));
  tmp=lives_strdup(lives_label_get_text(LIVES_LABEL(mainw->vps_label)));
  if (strlen(tmp)>55) memset(tmp+55,0,1);
  lives_label_set_text(LIVES_LABEL(mainw->vps_label),tmp);
  lives_free(tmp);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_widget_show(mainw->vps_label);
  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->vps_label, FALSE, FALSE, 0);

  mainw->spinbutton_pb_fps = lives_standard_spin_button_new(NULL,FALSE,1, -FPS_MAX, FPS_MAX, 0.1, 0.01, 3,
                             LIVES_BOX(mainw->framebar),_("Vary the video speed"));

  lives_widget_show(mainw->spinbutton_pb_fps);

  widget_opts.justify=LIVES_JUSTIFY_CENTER;
  if (palette->style==STYLE_PLAIN) {
    mainw->banner = lives_label_new("             = <  L i V E S > =                ");
  } else {
    mainw->banner = lives_label_new("                                                ");
  }
  widget_opts.justify=widget_opts.default_justify;

  lives_widget_show(mainw->banner);

  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->banner, TRUE, TRUE, 0);

  mainw->framecounter = lives_entry_new();
  lives_widget_show(mainw->framecounter);
  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->framecounter, FALSE, TRUE, 0);
  lives_entry_set_editable(LIVES_ENTRY(mainw->framecounter), FALSE);
  lives_entry_set_has_frame(LIVES_ENTRY(mainw->framecounter), FALSE);
  lives_entry_set_width_chars(LIVES_ENTRY(mainw->framecounter), 18);

  lives_widget_set_can_focus(mainw->framecounter, FALSE);

  mainw->curf_label = lives_standard_label_new("                                                            ");
  lives_widget_show(mainw->curf_label);
  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->curf_label, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->curf_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_widget_hide(mainw->framebar);

  hbox1 = lives_hbox_new(FALSE, 0);
  lives_widget_show(hbox1);
  lives_box_pack_start(LIVES_BOX(vbox4), hbox1, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(hbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  lives_widget_set_vexpand(hbox1,FALSE);

  mainw->eventbox3 = lives_event_box_new();
  lives_widget_show(mainw->eventbox3);
  lives_box_pack_start(LIVES_BOX(hbox1), mainw->eventbox3, TRUE, FALSE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  mainw->frame1 = lives_frame_new(NULL);
  lives_widget_show(mainw->frame1);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->frame1), widget_opts.border_width);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox3), mainw->frame1);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->frame1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->frame1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }
  lives_widget_set_vexpand(mainw->frame1,FALSE);
  lives_widget_set_hexpand(mainw->frame1,FALSE);

  lives_frame_set_shadow_type(LIVES_FRAME(mainw->frame1), LIVES_SHADOW_NONE);

  mainw->freventbox0=lives_event_box_new();
  lives_widget_show(mainw->freventbox0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->freventbox0, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  lives_widget_set_vexpand(mainw->freventbox0,FALSE);
  lives_widget_set_hexpand(mainw->freventbox0,FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->frame1), mainw->freventbox0);
  lives_widget_set_app_paintable(mainw->freventbox0,TRUE);

  lives_widget_show(mainw->start_image);
  lives_container_add(LIVES_CONTAINER(mainw->freventbox0), mainw->start_image);
  lives_widget_set_vexpand(mainw->start_image,FALSE);
  lives_widget_set_hexpand(mainw->start_image,FALSE);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->start_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  label = lives_standard_label_new(_("First Frame"));
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }
  lives_widget_show(label);
  lives_frame_set_label_widget(LIVES_FRAME(mainw->frame1), label);

  mainw->playframe = lives_frame_new(NULL);
  lives_widget_hide(mainw->playframe);
  lives_box_pack_start(LIVES_BOX(hbox1), mainw->playframe, TRUE, FALSE, 0);
  lives_widget_set_size_request(mainw->playframe, DEFAULT_FRAME_HSIZE, DEFAULT_FRAME_VSIZE);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->playframe), widget_opts.border_width);

  lives_frame_set_shadow_type(LIVES_FRAME(mainw->playframe), LIVES_SHADOW_NONE);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->playframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  pf_label = lives_standard_label_new(_("Play"));
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(pf_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }
  lives_widget_show(pf_label);
  lives_frame_set_label_widget(LIVES_FRAME(mainw->playframe), pf_label);

  mainw->pl_eventbox = lives_event_box_new();
  lives_widget_set_bg_color(mainw->pl_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_container_add(LIVES_CONTAINER(mainw->playframe), mainw->pl_eventbox);
  lives_widget_show(mainw->pl_eventbox);

  mainw->playarea = lives_hbox_new(FALSE,0);

  lives_container_add(LIVES_CONTAINER(mainw->pl_eventbox), mainw->playarea);

  lives_widget_set_app_paintable(mainw->pl_eventbox,TRUE);

  mainw->eventbox4 = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(hbox1), mainw->eventbox4, TRUE, FALSE, 0);
  lives_widget_set_bg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_vexpand(mainw->eventbox4,FALSE);
  lives_widget_set_hexpand(mainw->eventbox4,FALSE);
  lives_widget_show(mainw->eventbox4);

  mainw->frame2 = lives_frame_new(NULL);
  lives_widget_show(mainw->frame2);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->frame2), widget_opts.border_width);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox4), mainw->frame2);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->frame2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  lives_widget_set_vexpand(mainw->frame2,FALSE);
  lives_widget_set_hexpand(mainw->frame2,FALSE);

  lives_frame_set_shadow_type(LIVES_FRAME(mainw->frame2), LIVES_SHADOW_NONE);

  mainw->freventbox1=lives_event_box_new();
  lives_widget_show(mainw->freventbox1);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->freventbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  lives_widget_set_vexpand(mainw->freventbox1,FALSE);
  lives_widget_set_hexpand(mainw->freventbox1,FALSE);
  lives_widget_set_app_paintable(mainw->freventbox1,TRUE);

  lives_container_add(LIVES_CONTAINER(mainw->frame2), mainw->freventbox1);

  lives_widget_show(mainw->end_image);
  lives_container_add(LIVES_CONTAINER(mainw->freventbox1), mainw->end_image);
  lives_widget_set_vexpand(mainw->end_image,FALSE);
  lives_widget_set_hexpand(mainw->end_image,FALSE);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->end_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  // default frame sizes
  mainw->def_width=DEFAULT_FRAME_HSIZE;
  mainw->def_height=DEFAULT_FRAME_HSIZE;

  if (!(mainw->imframe==NULL)) {
    if (lives_pixbuf_get_width(mainw->imframe)+H_RESIZE_ADJUST<mainw->def_width) {
      mainw->def_width=lives_pixbuf_get_width(mainw->imframe)+H_RESIZE_ADJUST;
    }
    if (lives_pixbuf_get_height(mainw->imframe)+V_RESIZE_ADJUST*mainw->foreign<mainw->def_height) {
      mainw->def_height=lives_pixbuf_get_height(mainw->imframe)+V_RESIZE_ADJUST*mainw->foreign;
    }
  }

  // the actual playback image for the internal player
  mainw->play_image = lives_image_new_from_pixbuf(NULL);

  lives_widget_show(mainw->play_image);
  lives_object_ref(mainw->play_image);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->play_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  lives_widget_set_hexpand(mainw->play_image,TRUE);
  lives_widget_set_vexpand(mainw->play_image,TRUE);

  lives_object_ref_sink(mainw->play_image);

  label = lives_standard_label_new(_("Last Frame"));
  lives_widget_show(label);
  lives_frame_set_label_widget(LIVES_FRAME(mainw->frame2), label);
  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  hbox3 = lives_hbox_new(FALSE, 0);
  lives_widget_show(hbox3);
  lives_box_pack_start(LIVES_BOX(vbox4), hbox3, FALSE, TRUE, 0);

  dpw=widget_opts.packing_width;
  woat=widget_opts.apply_theme;
  widget_opts.expand=LIVES_EXPAND_EXTRA;
  widget_opts.apply_theme=FALSE;
  widget_opts.packing_width=MAIN_SPIN_SPACER;
  mainw->spinbutton_start = lives_standard_spin_button_new(NULL,FALSE,0., 0., 0., 1., 100.,0,
                            LIVES_BOX(hbox3),_("The first selected frame in this clip"));
  widget_opts.expand=LIVES_EXPAND_DEFAULT;
  widget_opts.packing_width=dpw;
  widget_opts.apply_theme=woat;

  lives_widget_show(mainw->spinbutton_start);

  mainw->arrow1 = lives_arrow_new(LIVES_ARROW_LEFT, LIVES_SHADOW_OUT);
  lives_widget_show(mainw->arrow1);
  lives_box_pack_start(LIVES_BOX(hbox3), mainw->arrow1, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->arrow1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_entry_set_width_chars(LIVES_ENTRY(mainw->spinbutton_start),12);
  mainw->sel_label = lives_standard_label_new(NULL);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->sel_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  set_sel_label(mainw->sel_label);
  lives_widget_show(mainw->sel_label);
  lives_box_pack_start(LIVES_BOX(hbox3), mainw->sel_label, FALSE, FALSE, 0);

  mainw->arrow2 = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  lives_widget_show(mainw->arrow2);
  lives_box_pack_start(LIVES_BOX(hbox3), mainw->arrow2, FALSE, FALSE, 0);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->arrow2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  widget_opts.expand=LIVES_EXPAND_EXTRA;
  widget_opts.packing_width=MAIN_SPIN_SPACER;
  widget_opts.apply_theme=FALSE;
  mainw->spinbutton_end = lives_standard_spin_button_new(NULL,FALSE,0., 0., 0., 1., 100.,0,
                          LIVES_BOX(hbox3),_("The last selected frame in this clip"));
  widget_opts.expand=LIVES_EXPAND_DEFAULT;
  widget_opts.packing_width=dpw;
  widget_opts.apply_theme=woat;

  lives_widget_show(mainw->spinbutton_end);


  lives_entry_set_width_chars(LIVES_ENTRY(mainw->spinbutton_end),12);

  if (palette->style&STYLE_1&&palette->style&STYLE_2) {
#if !GTK_CHECK_VERSION(3,0,0)
    // background colour seems to be broken in gtk+3 !!!
    lives_widget_set_base_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_base_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
    lives_widget_set_base_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_base_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_back);
    lives_widget_set_text_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_text_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
    lives_widget_set_text_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_text_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, &palette->normal_fore);
#endif
    lives_widget_set_fg_color(mainw->sel_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_widget_set_sensitive(mainw->spinbutton_start,FALSE);
  lives_widget_set_sensitive(mainw->spinbutton_end,FALSE);

  mainw->hseparator = lives_hseparator_new();

  if (palette->style&STYLE_1) {
    lives_box_pack_start(LIVES_BOX(vbox4), mainw->sep_image, FALSE, TRUE, widget_opts.packing_height*2);
    lives_widget_show(mainw->sep_image);
  }

  else {
    lives_box_pack_start(LIVES_BOX(vbox4), mainw->hseparator, TRUE, TRUE, 0);
    lives_widget_show(mainw->hseparator);
  }

  mainw->eventbox5 = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->eventbox5, FALSE, FALSE, 0);
  lives_widget_show(mainw->eventbox5);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->eventbox5, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

#ifdef ENABLE_GIW_3
  mainw->hruler=giw_timeline_new(LIVES_ORIENTATION_HORIZONTAL);
#else
  mainw->hruler = lives_standard_hruler_new();
#endif
  lives_ruler_set_range(LIVES_RULER(mainw->hruler), 0., 1000000., 0., 1000000.);

  lives_widget_set_size_request(mainw->hruler, -1, CE_HRULE_HEIGHT);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox5), mainw->hruler);

  lives_widget_add_events(mainw->eventbox5, LIVES_POINTER_MOTION_MASK | LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK |
                          LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK);

  mainw->eventbox2 = lives_event_box_new();
  lives_widget_show(mainw->eventbox2);
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->eventbox2, TRUE, TRUE, 0);
  lives_widget_add_events(mainw->eventbox2, LIVES_BUTTON1_MOTION_MASK | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK);

  lives_widget_set_vexpand(mainw->eventbox2,TRUE);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->eventbox2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_bg_color(mainw->hruler, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->hruler, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_widget_show(vbox2);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox2), vbox2);

  mainw->vidbar = lives_standard_label_new(_("Video"));

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->vidbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  if (palette->style==STYLE_PLAIN) {
    lives_widget_show(mainw->vidbar);
  } else {
    lives_widget_set_bg_color(vbox2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_hide(mainw->vidbar);
  }

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->vidbar, TRUE, TRUE, 0);

  mainw->video_draw = lives_drawing_area_new();
  lives_widget_set_app_paintable(mainw->video_draw,TRUE);
  lives_widget_set_size_request(mainw->video_draw,lives_widget_get_allocation_width(mainw->LiVES),CE_VIDBAR_HEIGHT);
  lives_widget_show(mainw->video_draw);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->video_draw, TRUE, TRUE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->video_draw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  mainw->laudbar = lives_standard_label_new(_("Left Audio"));

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->laudbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->laudbar, TRUE, TRUE, 0);

  if (palette->style==STYLE_PLAIN) {
    lives_widget_show(mainw->laudbar);
  } else {
    lives_widget_hide(mainw->laudbar);
  }

  mainw->laudio_draw = lives_drawing_area_new();
  lives_widget_set_app_paintable(mainw->laudio_draw,TRUE);
  lives_widget_set_size_request(mainw->laudio_draw,lives_widget_get_allocation_width(mainw->LiVES),CE_VIDBAR_HEIGHT);
  lives_widget_show(mainw->laudio_draw);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->laudio_draw, TRUE, TRUE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->laudio_draw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  mainw->raudbar = lives_standard_label_new(_("Right Audio"));

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(mainw->raudbar, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->raudbar, TRUE, TRUE, 0);

  if (palette->style==STYLE_PLAIN) {
    lives_widget_show(mainw->raudbar);
  } else {
    lives_widget_hide(mainw->raudbar);
  }

  mainw->raudio_draw = lives_drawing_area_new();
  lives_widget_set_app_paintable(mainw->raudio_draw,TRUE);
  lives_widget_set_size_request(mainw->raudio_draw,lives_widget_get_allocation_width(mainw->LiVES),CE_VIDBAR_HEIGHT);
  lives_widget_show(mainw->raudio_draw);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->raudio_draw, TRUE, TRUE, 0);
  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->raudio_draw, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  eventbox=lives_event_box_new();
  lives_widget_set_vexpand(eventbox,TRUE);
  lives_widget_show(eventbox);

  mainw->message_box=lives_vbox_new(FALSE, 0);
  lives_widget_show(mainw->message_box);

  lives_widget_set_vexpand(mainw->message_box,TRUE);

  lives_box_pack_start(LIVES_BOX(mainw->vbox1), eventbox, TRUE, TRUE, 0);
  lives_container_add(LIVES_CONTAINER(eventbox), mainw->message_box);

  mainw->textview1=NULL;
  mainw->scrolledwindow=NULL;
  add_message_scroller(mainw->message_box);

  // accel keys
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Page_Up, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(prevclip_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Page_Down, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(nextclip_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Down, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(slower_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Up, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(faster_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Left, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(skip_back_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Right, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(skip_forward_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Space, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(dirchange_callback),LIVES_INT_TO_POINTER(TRUE),NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Return, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(fps_reset_callback),LIVES_INT_TO_POINTER(TRUE),NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_k, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_t, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(textparm_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_m, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(rtemode_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_x, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(swap_fg_bg_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_n, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(nervous_callback),NULL,NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_w, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(show_sync_callback),NULL,NULL));

  if (FN_KEYS>0) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F1, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(1),NULL));
    if (FN_KEYS>1) {
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F2, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(2),NULL));
      if (FN_KEYS>2) {
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F3, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(3),NULL));
        if (FN_KEYS>3) {
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F4, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(4),NULL));
          if (FN_KEYS>4) {
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F5, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(5),NULL));
            if (FN_KEYS>5) {
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F6, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(6),NULL));
              if (FN_KEYS>6) {
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F7, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(7),NULL));
                if (FN_KEYS>7) {
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F8, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(8),NULL));
                  if (FN_KEYS>8) {
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F9, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(9),NULL));
                    if (FN_KEYS>9) {
                      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F10, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(10),NULL));
                      if (FN_KEYS>10) {
                        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F11, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(11),NULL));
                        if (FN_KEYS>11) {
                          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F12, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),LIVES_INT_TO_POINTER(12),NULL));
                          // ad nauseum...
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_0, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(0),NULL));
  if (FX_KEYS_PHYSICAL>0) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_1, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(1),NULL));
    if (FX_KEYS_PHYSICAL>1) {
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_2, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(2),NULL));
      if (FX_KEYS_PHYSICAL>2) {
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_3, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(3),NULL));
        if (FX_KEYS_PHYSICAL>3) {
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_4, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(4),NULL));
          if (FX_KEYS_PHYSICAL>4) {
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_5, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(5),NULL));
            if (FX_KEYS_PHYSICAL>5) {
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_6, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(6),NULL));
              if (FX_KEYS_PHYSICAL>6) {
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_7, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(7),NULL));
                if (FX_KEYS_PHYSICAL>7) {
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_8, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(8),NULL));
                  if (FX_KEYS_PHYSICAL>8) {
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_9, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),LIVES_INT_TO_POINTER(9),NULL));
                  }
                }
              }
            }
          }
        }
      }
    }
  }




  lives_text_view_set_text(LIVES_TEXT_VIEW(mainw->textview1),
                           _("Starting...\n"), -1);

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->LiVES), LIVES_WIDGET_DELETE_EVENT,
                       LIVES_GUI_CALLBACK(on_LiVES_delete_event),
                       NULL);

  mainw->config_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->video_draw), LIVES_WIDGET_CONFIGURE_EVENT,
                     LIVES_GUI_CALLBACK(config_event),
                     NULL);
  mainw->vidbar_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->video_draw), LIVES_WIDGET_EXPOSE_EVENT,
                     LIVES_GUI_CALLBACK(expose_vid_event),
                     NULL);
  mainw->laudbar_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->laudio_draw), LIVES_WIDGET_EXPOSE_EVENT,
                      LIVES_GUI_CALLBACK(expose_laud_event),
                      NULL);
  mainw->raudbar_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->raudio_draw), LIVES_WIDGET_EXPOSE_EVENT,
                      LIVES_GUI_CALLBACK(expose_raud_event),
                      NULL);
  mainw->pb_fps_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_pb_fps), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                     LIVES_GUI_CALLBACK(changed_fps_during_pb),
                     NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_sel_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_dvd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_vcd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(2));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_loc), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_loc_activate),
                       NULL);
#ifdef HAVE_WEBM
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_utube), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_utube_activate),
                       NULL);
#endif

#ifdef HAVE_LDVGRAB
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_firewire), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_fw_activate),
                       LIVES_INT_TO_POINTER(CAM_FORMAT_DV));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_hfirewire), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_fw_activate),
                       LIVES_INT_TO_POINTER(CAM_FORMAT_HDV));
#endif

#ifdef HAVE_YUV4MPEG
  if (capable->smog_version_correct) {
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_yuv4m), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_open_yuv4m_activate),
                         NULL);
  }
#endif
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_lives2lives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_lives2lives_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->send_lives2lives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_send_lives2lives_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recent1), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recent2), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(2));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recent3), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(3));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recent4), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recent_activate),
                       LIVES_INT_TO_POINTER(4));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->backup), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_backup_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->restore), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_restore_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->save_as), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_as_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->save_selection), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_selection_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->close), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_close_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->import_proj), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_import_proj_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->export_proj), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_export_proj_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->clear_ds), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cleardisk_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->quit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_quit_activate),
                       LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_save_set), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_quit_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->undo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_undo_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->redo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_redo_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->copy), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_copy_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->mt_menu), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_multitrack_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->cut), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cut_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->insert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insert_pre_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->merge), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_merge_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->paste_as_new), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_paste_as_new_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->xdelete), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_delete_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_all), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_all_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_start_only), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_start_only_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_end_only), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_end_only_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_invert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_invert_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_new), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_new_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_to_end), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_to_end_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_from_start), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_from_start_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_last), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_last_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->lock_selwidth), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_lock_selwidth_activate),
                       NULL);
  mainw->record_perf_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->record_perf), LIVES_WIDGET_ACTIVATE_SIGNAL,
                          LIVES_GUI_CALLBACK(on_record_perf_activate),
                          NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playall), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rewind), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rewind_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playsel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playsel_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playclip), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playclip_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->stop), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_stop_activate),
                       NULL);
  mainw->fullscreen_cb_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->full_screen), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_full_screen_activate),
                            NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->sw_sound), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &mainw->save_with_sound); // TODO - make pref
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->showsubs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_showsubs_toggled),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->letter), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &prefs->letterbox);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->aload_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &prefs->autoload_subs);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->ccpd_sound), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_boolean_toggled),
                       &mainw->ccpd_with_sound); // TODO - make pref
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->dsize), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_double_size_activate),
                       NULL);
  mainw->sepwin_cb_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->sepwin), LIVES_WIDGET_ACTIVATE_SIGNAL,
                        LIVES_GUI_CALLBACK(on_sepwin_activate),
                        NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->fade), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fade_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->loop_video), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_loop_video_activate),
                       NULL);
  mainw->loop_cont_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->loop_continue), LIVES_WIDGET_ACTIVATE_SIGNAL,
                        LIVES_GUI_CALLBACK(on_loop_cont_activate),
                        NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->loop_ping_pong), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_ping_pong_activate),
                       NULL);
  mainw->mute_audio_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->mute_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_mute_activate),
                         NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->sticky), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_sticky_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->showfct), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_showfct_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->preferences), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_preferences_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->change_speed), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_change_speed_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->resample_video), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_resample_video_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->load_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_load_subs_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->erase_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_erase_subs_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->capture), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_capture_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rev_clipboard), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rev_clipboard_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->export_selaudio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_export_audio_activate),
                       LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->export_allaudio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_export_audio_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->append_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_append_audio_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->trim_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_trim_audio_activate),
                       LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->trim_to_pstart), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_trim_audio_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delsel_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_del_audio_activate),
                       LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->fade_aud_in), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fade_audio_activate),
                       LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->fade_aud_out), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fade_audio_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delall_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_del_audio_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->ins_silence), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_ins_silence_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recaudio_clip), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recaudclip_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recaudio_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recaudsel_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->resample_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_resample_audio_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->load_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_load_audio_activate),
                       NULL);
  if (capable->has_cdda2wav) {
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->load_cdtrack), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_cdtrack_activate),
                         NULL);

    lives_signal_connect(LIVES_GUI_OBJECT(mainw->eject_cd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_eject_cd_activate),
                         NULL);
  }

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->show_file_info), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_file_info_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->show_file_comments), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_file_comments_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->show_clipboard_info), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_clipboard_info_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->show_messages), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_messages_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->show_layout_errors), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(popup_lmap_errors),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rename), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rename_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(new_test_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_new_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
  lives_signal_connect(LIVES_GUI_OBJECT(copy_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_copy_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->edit_test_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_edit_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rename_test_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rename_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
  lives_signal_connect(LIVES_GUI_OBJECT(rebuild_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rebuild_rfx_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delete_test_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_delete_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delete_custom_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_delete_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_CUSTOM));
  lives_signal_connect(LIVES_GUI_OBJECT(import_custom_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_import_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_CUSTOM));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->export_custom_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_export_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_CUSTOM));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->promote_test_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_promote_rfx_activate),
                       LIVES_INT_TO_POINTER(RFX_STATUS_TEST));

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_load_set), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_load_set_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_show_keys), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_show_keys_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(assign_rte_keys), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_assign_rte_keys_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->save_rte_defs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_rte_defs_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_reset), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vj_reset_activate),
                       NULL);
#ifdef ENABLE_OSC
  lives_signal_connect(LIVES_GUI_OBJECT(midi_learn), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_midi_learn_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(midi_save), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_midi_save_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(midi_load), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_midi_load_activate),
                       NULL);
#endif

  mainw->toy_func_none=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->toy_none), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_toy_activate),
                       NULL);

  mainw->toy_func_autolives=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->toy_autolives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_toy_activate),
                            LIVES_INT_TO_POINTER(LIVES_TOY_AUTOLIVES));

  mainw->toy_func_random_frames=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->toy_random_frames), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                LIVES_GUI_CALLBACK(on_toy_activate),
                                LIVES_INT_TO_POINTER(LIVES_TOY_MAD_FRAMES));

  mainw->toy_func_lives_tv=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->toy_tv), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(on_toy_activate),
                           LIVES_INT_TO_POINTER(LIVES_TOY_TV));

  lives_signal_connect(LIVES_GUI_OBJECT(about), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_about_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->troubleshoot), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_troubleshoot_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(show_manual), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(show_manual_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(email_author), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(email_author_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(donate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(donate_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(report_bug), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(report_bug_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(suggest_feature), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(suggest_feature_activate),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(help_translate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(help_translate_activate),
                       NULL);

  mainw->spin_start_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_start), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_spinbutton_start_value_changed),
                         NULL);
  mainw->spin_end_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_end), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_spinbutton_end_value_changed),
                       NULL);

  // these are for the menu transport buttons
  if (capable->smog_version_correct) {
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_sepwinbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_sepwin_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_playall_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_stopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_stop_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_playselbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_playsel_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_rewindbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_rewind_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_mutebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_mute_button_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_loopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_loop_button_activate),
                         NULL);

    // these are 'invisible' buttons for the key accelerators
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_stopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_stop_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_bckground), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_fade_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_sepwin), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_sepwin_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_double), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_double_size_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_fullscreen), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_full_screen_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_infobutton), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_show_file_info_activate),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_hide), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_toolbar_hide),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_slower), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_slower_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_faster), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_faster_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_back), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_back_pressed),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->t_forward), LIVES_WIDGET_CLICKED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_forward_pressed),
                         NULL);

    mainw->mouse_fn1=lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                                          LIVES_GUI_CALLBACK(on_mouse_sel_update),
                                          NULL);
    lives_signal_handler_block(mainw->eventbox2,mainw->mouse_fn1);
    mainw->mouse_blocked=TRUE;
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                         LIVES_GUI_CALLBACK(on_mouse_sel_reset),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(on_mouse_sel_start),
                         NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->hruler), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                         LIVES_GUI_CALLBACK(return_true),
                         NULL);
    mainw->hrule_func=lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                                           LIVES_GUI_CALLBACK(on_hrule_update),
                                           NULL);
    lives_signal_handler_block(mainw->eventbox5,mainw->hrule_func);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_ENTER_EVENT,LIVES_GUI_CALLBACK(on_hrule_enter),NULL);

  }

  mainw->hrule_blocked=TRUE;
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                       LIVES_GUI_CALLBACK(on_hrule_reset),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(on_hrule_set),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox3), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(frame_context),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->eventbox4), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(frame_context),
                       LIVES_INT_TO_POINTER(2));

  lives_window_add_accel_group(LIVES_WINDOW(mainw->LiVES), mainw->accel_group);
  mainw->laudio_drawable=NULL;
  mainw->raudio_drawable=NULL;
  mainw->video_drawable=NULL;
  mainw->plug=NULL;

  lives_widget_set_can_focus(mainw->LiVES, TRUE);
  lives_widget_grab_focus(mainw->textview1);

}


void set_interactive(boolean interactive) {
  LiVESList *list;

  if (!interactive) {
    lives_object_ref(mainw->menubar);
    lives_widget_unparent(mainw->menubar);
    lives_widget_hide(mainw->btoolbar);
    lives_widget_set_sensitive(mainw->menubar,FALSE);
    lives_widget_set_sensitive(mainw->btoolbar,FALSE);
    if (mainw->multitrack!=NULL) {
      lives_object_ref(mainw->multitrack->menubar);
      lives_widget_unparent(mainw->multitrack->menubar);
      lives_widget_set_sensitive(mainw->multitrack->menubar,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_start,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_end,FALSE);
      lives_widget_set_sensitive(mainw->m_playbutton,FALSE);
      lives_widget_set_sensitive(mainw->m_stopbutton,FALSE);
      lives_widget_set_sensitive(mainw->m_loopbutton,FALSE);
      lives_widget_set_sensitive(mainw->m_rewindbutton,FALSE);
      lives_widget_set_sensitive(mainw->m_sepwinbutton,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar2,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar3,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->insa_checkbutton,FALSE);
      lives_widget_set_sensitive(mainw->multitrack->snapo_checkbutton,FALSE);
      list=mainw->multitrack->cb_list;
      while (list!=NULL) {
        lives_widget_set_sensitive((LiVESWidget *)list->data,FALSE);
        list=list->next;
      }
    }
    lives_widget_set_sensitive(mainw->spinbutton_start,FALSE);
    lives_widget_set_sensitive(mainw->spinbutton_end,FALSE);

    if (mainw->current_file>-1&&cfile!=NULL&&cfile->proc_ptr!=NULL) {
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->stop_button,FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button,FALSE);
      lives_widget_set_sensitive(cfile->proc_ptr->preview_button,FALSE);
    }

  } else {
    if (lives_widget_get_parent(mainw->menubar)==NULL) {
      lives_box_pack_start(LIVES_BOX(mainw->menu_hbox), mainw->menubar, FALSE, FALSE, 0);
      lives_object_unref(mainw->menubar);
    }
    lives_widget_show(mainw->btoolbar);
    lives_widget_set_sensitive(mainw->menubar,TRUE);
    lives_widget_set_sensitive(mainw->btoolbar,TRUE);
    if (mainw->multitrack!=NULL) {
      if (lives_widget_get_parent(mainw->multitrack->menubar)==NULL) {
        lives_box_pack_start(LIVES_BOX(mainw->multitrack->menu_hbox), mainw->multitrack->menubar, FALSE, FALSE, 0);
        lives_object_unref(mainw->multitrack->menubar);
      }
      lives_widget_show_all(mainw->multitrack->menubar);
      lives_widget_set_sensitive(mainw->multitrack->menubar,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_start,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_end,TRUE);
      lives_widget_set_sensitive(mainw->m_playbutton,TRUE);
      lives_widget_set_sensitive(mainw->m_stopbutton,TRUE);
      lives_widget_set_sensitive(mainw->m_loopbutton,TRUE);
      lives_widget_set_sensitive(mainw->m_rewindbutton,TRUE);
      lives_widget_set_sensitive(mainw->m_sepwinbutton,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar2,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar3,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->insa_checkbutton,TRUE);
      lives_widget_set_sensitive(mainw->multitrack->snapo_checkbutton,TRUE);
      list=mainw->multitrack->cb_list;
      while (list!=NULL) {
        lives_widget_set_sensitive((LiVESWidget *)list->data,TRUE);
        list=list->next;
      }
    }
    lives_widget_set_sensitive(mainw->spinbutton_start,TRUE);
    lives_widget_set_sensitive(mainw->spinbutton_end,TRUE);

    if (mainw->current_file>-1&&cfile!=NULL&&cfile->proc_ptr!=NULL) {
      lives_widget_set_sensitive(cfile->proc_ptr->cancel_button,TRUE);
      lives_widget_set_sensitive(cfile->proc_ptr->stop_button,TRUE);
      lives_widget_set_sensitive(cfile->proc_ptr->pause_button,TRUE);
      lives_widget_set_sensitive(cfile->proc_ptr->preview_button,TRUE);
    }

  }

  if (mainw->ce_thumbs) ce_thumbs_set_interactive(interactive);
  if (rte_window!=NULL) rte_window_set_interactive(interactive);

}




void fade_background(void) {

  if (palette->style==STYLE_PLAIN) {
    lives_label_set_text(LIVES_LABEL(mainw->banner),"            = <  L i V E S > =              ");
  }
  if (mainw->foreign) {
    lives_label_set_text(LIVES_LABEL(mainw->banner),_("    Press 'q' to stop recording.  DO NOT COVER THE PLAY WINDOW !   "));
    lives_widget_set_fg_color(mainw->banner, LIVES_WIDGET_STATE_NORMAL, &palette->banner_fade_text);
    lives_label_set_text(LIVES_LABEL(mainw->vps_label),("                      "));
  } else {
    lives_widget_set_bg_color(mainw->playframe, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
    if (mainw->sep_win) {
      lives_widget_hide(mainw->playframe);
    }
    lives_frame_set_shadow_type(LIVES_FRAME(mainw->playframe), LIVES_SHADOW_NONE);
  }

  lives_frame_set_label(LIVES_FRAME(mainw->playframe), "");

  lives_widget_set_fg_color(mainw->curf_label, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_fg_color(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->vbox1, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->LiVES, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(lives_widget_get_parent(mainw->message_box), LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);

  lives_widget_set_bg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->pl_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->play_image, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);

  lives_widget_set_bg_color(mainw->frame1, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->frame2, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->freventbox0, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_app_paintable(mainw->freventbox0,FALSE);

  lives_widget_set_bg_color(mainw->freventbox1, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_app_paintable(mainw->freventbox1,FALSE);

  lives_frame_set_shadow_type(LIVES_FRAME(mainw->frame1), LIVES_SHADOW_NONE);
  lives_frame_set_label(LIVES_FRAME(mainw->frame1), "");
  lives_frame_set_shadow_type(LIVES_FRAME(mainw->frame2), LIVES_SHADOW_NONE);
  lives_frame_set_label(LIVES_FRAME(mainw->frame2), "");

  if (mainw->toy_type!=LIVES_TOY_MAD_FRAMES||mainw->foreign) {
    lives_widget_hide(mainw->start_image);
    lives_widget_hide(mainw->end_image);
  }
  if (!mainw->foreign&&future_prefs->show_tool) {
    lives_widget_show(mainw->tb_hbox);
  }

  lives_widget_hide(mainw->menu_hbox);
  lives_widget_hide(mainw->hseparator);
  lives_widget_hide(mainw->sep_image);
  lives_widget_hide(mainw->scrolledwindow);
  lives_widget_hide(mainw->eventbox2);
  lives_widget_hide(mainw->spinbutton_start);
  lives_widget_hide(mainw->hruler);
  lives_widget_hide(mainw->eventbox5);
  lives_widget_hide(mainw->message_box);

  if (!mainw->foreign) {
    lives_widget_show(mainw->t_forward);
    lives_widget_show(mainw->t_back);
    lives_widget_show(mainw->t_slower);
    lives_widget_show(mainw->t_faster);
    lives_widget_show(mainw->t_fullscreen);
    lives_widget_show(mainw->t_sepwin);
  }

  lives_widget_hide(mainw->spinbutton_start);
  lives_widget_hide(mainw->spinbutton_end);
  lives_widget_hide(mainw->sel_label);
  lives_widget_hide(mainw->arrow1);
  lives_widget_hide(mainw->arrow2);

  // since the hidden menu buttons are not activable on some window managers
  // we need to remove the accelerators and add accelerator keys instead

  if (stop_closure==NULL) {
    lives_widget_remove_accelerator(mainw->stop, mainw->accel_group, LIVES_KEY_q, (LiVESXModifierType)0);
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_q, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                              (stop_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(stop_callback),NULL,NULL)));

    if (!mainw->foreign) {
      // TODO - do these checks in the end functions
      lives_widget_remove_accelerator(mainw->record_perf, mainw->accel_group, LIVES_KEY_r, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_r, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (rec_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(rec_callback),NULL,NULL)));

      lives_widget_remove_accelerator(mainw->full_screen, mainw->accel_group, LIVES_KEY_f, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_f, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (fullscreen_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(fullscreen_callback),NULL,NULL)));

      lives_widget_remove_accelerator(mainw->showfct, mainw->accel_group, LIVES_KEY_h, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_h, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (showfct_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(showfct_callback),NULL,NULL)));

      lives_widget_remove_accelerator(mainw->showsubs, mainw->accel_group, LIVES_KEY_v, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_v, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (showsubs_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(showsubs_callback),NULL,NULL)));

      lives_widget_remove_accelerator(mainw->sepwin, mainw->accel_group, LIVES_KEY_s, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_s, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (sepwin_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(sepwin_callback),NULL,NULL)));

      if (!cfile->achans||mainw->mute||mainw->loop_cont||is_realtime_aplayer(prefs->audio_player)) {
        lives_widget_remove_accelerator(mainw->loop_video, mainw->accel_group, LIVES_KEY_l, (LiVESXModifierType)0);
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_l, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  (loop_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(loop_callback),NULL,NULL)));

        lives_widget_remove_accelerator(mainw->loop_continue, mainw->accel_group, LIVES_KEY_o, (LiVESXModifierType)0);
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_o, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  (loop_cont_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(loop_cont_callback),NULL,NULL)));

      }
      lives_widget_remove_accelerator(mainw->loop_ping_pong, mainw->accel_group, LIVES_KEY_g, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_g, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (ping_pong_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(ping_pong_callback),NULL,NULL)));
      lives_widget_remove_accelerator(mainw->mute_audio, mainw->accel_group, LIVES_KEY_z, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_z, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (mute_audio_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(mute_audio_callback),NULL,NULL)));
      lives_widget_remove_accelerator(mainw->dsize, mainw->accel_group, LIVES_KEY_d, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_d, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (dblsize_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(dblsize_callback),NULL,NULL)));
      lives_widget_remove_accelerator(mainw->fade, mainw->accel_group, LIVES_KEY_b, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_b, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (fade_closure=lives_cclosure_new(LIVES_GUI_CALLBACK(fade_callback),NULL,NULL)));
    }
  }

  lives_widget_queue_draw(mainw->vbox1);
}






void unfade_background(void) {
  if (prefs->open_maximised&&prefs->show_gui) {
    lives_window_maximize(LIVES_WINDOW(mainw->LiVES));
  }

  if (palette->style==STYLE_PLAIN) {
    lives_label_set_text(LIVES_LABEL(mainw->banner),"   = <  L i V E S > =                            ");
  } else {
    lives_label_set_text(LIVES_LABEL(mainw->banner),"                                                ");
  }
  lives_widget_set_fg_color(mainw->banner, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_bg_color(mainw->eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->vbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_frame_set_label(LIVES_FRAME(mainw->frame1), _("First Frame"));
  if (!mainw->preview) {
    lives_frame_set_label(LIVES_FRAME(mainw->playframe),_("Play"));
  } else {
    lives_frame_set_label(LIVES_FRAME(mainw->playframe),_("Preview"));
  }

  lives_frame_set_label(LIVES_FRAME(mainw->frame2), _("Last Frame"));
  lives_widget_set_fg_color(mainw->curf_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_fg_color(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_bg_color(mainw->LiVES, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mainw->pl_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  if (palette->style&STYLE_1) {
    lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->playframe)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame1)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_fg_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame2)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_widget_set_fg_color(mainw->curf_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->vbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->LiVES, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(lives_widget_get_parent(mainw->message_box), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->pl_eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->play_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mainw->frame1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->frame2, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->freventbox0, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->freventbox1, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->play_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  lives_widget_set_bg_color(mainw->playframe, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_show(mainw->menu_hbox);
  lives_widget_hide(mainw->tb_hbox);
  lives_widget_show(mainw->hseparator);
  if (!mainw->double_size||mainw->sep_win) {
    if (palette->style&STYLE_1) {
      lives_widget_show(mainw->sep_image);
    }
    if (mainw->multitrack==NULL) lives_widget_show(mainw->scrolledwindow);
  }
  lives_widget_show(mainw->start_image);
  lives_widget_show(mainw->end_image);
  lives_widget_show(mainw->eventbox2);
  if (!cfile->opening) {
    lives_widget_show(mainw->hruler);
    lives_widget_show(mainw->eventbox5);
  }
  if (cfile->frames>0&&!(prefs->sepwin_type==SEPWIN_TYPE_STICKY&&mainw->sep_win)) {
    lives_widget_show(mainw->playframe);
  }
  lives_widget_show(mainw->spinbutton_start);
  lives_widget_show(mainw->spinbutton_end);
  lives_widget_show(mainw->sel_label);
  lives_widget_show(mainw->arrow1);
  lives_widget_show(mainw->arrow2);
  lives_widget_show(mainw->spinbutton_pb_fps);
  lives_widget_show(mainw->message_box);
  lives_widget_set_fg_color(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_fg_color(mainw->curf_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);

  if (stop_closure!=NULL&&prefs->show_gui) {
    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), stop_closure);
    lives_widget_add_accelerator(mainw->stop, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_q, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), rec_closure);
    lives_widget_add_accelerator(mainw->record_perf, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_r, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), fullscreen_closure);
    lives_widget_add_accelerator(mainw->full_screen, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_f, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);
    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), sepwin_closure);
    lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_s, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), mute_audio_closure);
    lives_widget_add_accelerator(mainw->mute_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_z, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), ping_pong_closure);
    lives_widget_add_accelerator(mainw->loop_ping_pong, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_g, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    if (!cfile->achans||mainw->mute||mainw->loop_cont||is_realtime_aplayer(prefs->audio_player)) {
      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), loop_closure);
      lives_widget_add_accelerator(mainw->loop_video, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                   LIVES_KEY_l, (LiVESXModifierType)0,
                                   LIVES_ACCEL_VISIBLE);
      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), loop_cont_closure);
      lives_widget_add_accelerator(mainw->loop_continue, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                   LIVES_KEY_o, (LiVESXModifierType)0,
                                   LIVES_ACCEL_VISIBLE);
      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), showfct_closure);
    }
    lives_widget_add_accelerator(mainw->showfct, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_h, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), showsubs_closure);
    lives_widget_add_accelerator(mainw->showsubs, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_v, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), sepwin_closure);
    lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_s, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), dblsize_closure);
    lives_widget_add_accelerator(mainw->dsize, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_d, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), fade_closure);
    lives_widget_add_accelerator(mainw->fade, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_b, (LiVESXModifierType)0,
                                 LIVES_ACCEL_VISIBLE);

    stop_closure=NULL;

  }
  if (mainw->double_size) {
    resize(2);
  } else {
    resize(1);
  }

  lives_widget_context_update();

  lives_widget_set_app_paintable(mainw->freventbox0,TRUE);
  lives_widget_set_app_paintable(mainw->freventbox1,TRUE);
}



void fullscreen_internal(void) {
  // resize for full screen, internal player, no separate window

  int bx,by;

  int w,h,scr_width,scr_height;

  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
    scr_height=mainw->scr_height;
  } else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  if (mainw->multitrack==NULL) {

    lives_widget_hide(mainw->frame1);
    lives_widget_hide(mainw->frame2);

    lives_widget_hide(mainw->eventbox3);
    lives_widget_hide(mainw->eventbox4);

    lives_frame_set_label(LIVES_FRAME(mainw->playframe),NULL);
    lives_container_set_border_width(LIVES_CONTAINER(mainw->playframe), 0);

    lives_widget_hide(mainw->t_bckground);
    lives_widget_hide(mainw->t_double);

    lives_widget_hide(mainw->menu_hbox);


    if (mainw->playing_file==-1) {
      lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->play_image),NULL);
    }

    lives_widget_context_update();

    get_border_size(mainw->LiVES, &bx, &by);

    if (future_prefs->show_tool) by+=lives_widget_get_allocation_height(mainw->tb_hbox);

    lives_widget_set_size_request(mainw->playframe, lives_widget_get_allocation_width(mainw->LiVES)-bx,
                                  lives_widget_get_allocation_height(mainw->LiVES)-by);

    w=lives_widget_get_allocation_width(mainw->LiVES);
    h=lives_widget_get_allocation_height(mainw->LiVES);

    if (w>scr_width) w=scr_width;
    if (h>scr_height) h=scr_height;

    lives_widget_set_size_request(mainw->LiVES,w,h);

  } else {
    make_play_window();
  }

}



void block_expose(void) {
  // block expose/config events
  // sometimes we need to do this before showing/re-showing the play window
  // otherwise we get into a loop of expose events
  // symptoms - strace shows the app looping on poll() and it is otherwise
  // unresponsive
  mainw->draw_blocked=TRUE;
#if !GTK_CHECK_VERSION(3,0,0)
  lives_signal_handler_block(mainw->video_draw,mainw->config_func);
  lives_signal_handler_block(mainw->video_draw,mainw->vidbar_func);
  lives_signal_handler_block(mainw->laudio_draw,mainw->laudbar_func);
  lives_signal_handler_block(mainw->raudio_draw,mainw->raudbar_func);
#endif
}

void unblock_expose(void) {
  // unblock expose/config events
  mainw->draw_blocked=FALSE;
#if !GTK_CHECK_VERSION(3,0,0)
  lives_signal_handler_unblock(mainw->video_draw,mainw->config_func);
  lives_signal_handler_unblock(mainw->video_draw,mainw->vidbar_func);
  lives_signal_handler_unblock(mainw->laudio_draw,mainw->laudbar_func);
  lives_signal_handler_unblock(mainw->raudio_draw,mainw->raudbar_func);
#endif
}




void make_preview_box(void) {
  // create a box to show frames in, this will go in the sepwin when we are not playing
  LiVESWidget *hbox;
  LiVESWidget *hbox_buttons;
  LiVESWidget *radiobutton_free;
  LiVESWidget *radiobutton_start;
  LiVESWidget *radiobutton_end;
  LiVESWidget *radiobutton_ptr;
  LiVESWidget *rewind_img;
  LiVESWidget *playsel_img;
  LiVESWidget *play_img;
  LiVESWidget *loop_img;
  LiVESWidget *eventbox;

  LiVESSList *radiobutton_group = NULL;

  char buff[PATH_MAX];
  char *fnamex;
  char *tmp,*tmp2;

  mainw->preview_box = lives_vbox_new(FALSE, 0);
  lives_object_ref(mainw->preview_box);

  eventbox=lives_event_box_new();
  lives_widget_set_events(eventbox, LIVES_SCROLL_MASK);

  lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_SCROLL_EVENT,
                       LIVES_GUI_CALLBACK(on_mouse_scroll),
                       NULL);

#if GTK_CHECK_VERSION(3,0,0)
  lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_EXPOSE_EVENT,
                       LIVES_GUI_CALLBACK(expose_pim),
                       NULL);
#endif

  lives_box_pack_start(LIVES_BOX(mainw->preview_box), eventbox, TRUE, TRUE, 0);

  lives_signal_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                       LIVES_GUI_CALLBACK(frame_context),
                       LIVES_INT_TO_POINTER(3));


  mainw->preview_image = lives_image_new_from_pixbuf(NULL);
  lives_widget_show(mainw->preview_image);
  lives_container_add(LIVES_CONTAINER(eventbox), mainw->preview_image);
  lives_widget_set_app_paintable(eventbox,TRUE);

  if (mainw->play_window!=NULL) {
    if (mainw->current_file<0||cfile->frames==0) {
      if (mainw->imframe!=NULL) {
        lives_widget_set_size_request(mainw->preview_image,lives_pixbuf_get_width(mainw->imframe),lives_pixbuf_get_height(mainw->imframe));

      }
    } else
      lives_widget_set_size_request(mainw->preview_image,MAX(mainw->pwidth,mainw->sepwin_minwidth),mainw->pheight);
  }

  lives_widget_set_hexpand(mainw->preview_box,TRUE);
  lives_widget_set_vexpand(mainw->preview_box,TRUE);

  mainw->preview_controls=lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->preview_box), mainw->preview_controls, FALSE, FALSE, 0);
  lives_widget_set_vexpand(mainw->preview_controls,FALSE);

  hbox = lives_hbox_new(FALSE, 0);
  lives_widget_set_vexpand(hbox,FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(hbox), 0);


  mainw->preview_spinbutton = lives_standard_spin_button_new(NULL,FALSE,(mainw->current_file>-1&&cfile->frames>0.)?1.:0.,
                              (mainw->current_file>-1&&cfile->frames>0.)?1.:0.,
                              (mainw->current_file>-1&&cfile->frames>0.)?cfile->frames:0.,
                              1., 10., 0,
                              LIVES_BOX(hbox),_("Frame number to preview"));

  mainw->preview_scale=lives_hscale_new(lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(mainw->preview_spinbutton)));
  lives_scale_set_draw_value(LIVES_SCALE(mainw->preview_scale),FALSE);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->preview_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_bg_color(eventbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    //lives_widget_set_bg_color(mainw->preview_scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    lives_widget_set_fg_color(mainw->preview_scale, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  }

  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), mainw->preview_scale,FALSE, FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), hbox, FALSE, FALSE, 0);

  lives_entry_set_width_chars(LIVES_ENTRY(mainw->preview_spinbutton),8);

  radiobutton_free=lives_standard_radio_button_new((tmp=lives_strdup(_("_Free"))),TRUE,radiobutton_group,LIVES_BOX(hbox),
                   (tmp2=lives_strdup(_("Free choice of frame number"))));
  lives_free(tmp);
  lives_free(tmp2);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_free));

  radiobutton_start=lives_standard_radio_button_new((tmp=lives_strdup(_("_Start"))),TRUE,radiobutton_group,LIVES_BOX(hbox),
                    (tmp2=lives_strdup(_("Frame number is linked to start frame"))));
  lives_free(tmp);
  lives_free(tmp2);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_start));

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_start), mainw->prv_link==PRV_START);


  radiobutton_end=lives_standard_radio_button_new((tmp=lives_strdup(_("_End"))),TRUE,radiobutton_group,LIVES_BOX(hbox),
                  (tmp2=lives_strdup(_("Frame number is linked to end frame"))));
  lives_free(tmp);
  lives_free(tmp2);
  radiobutton_group = lives_radio_button_get_group(LIVES_RADIO_BUTTON(radiobutton_end));

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_end), mainw->prv_link==PRV_END);


  radiobutton_ptr=lives_standard_radio_button_new((tmp=lives_strdup(_("_Pointer"))),TRUE,radiobutton_group,LIVES_BOX(hbox),
                  (tmp2=lives_strdup(_("Frame number is linked to playback pointer"))));
  lives_free(tmp);
  lives_free(tmp2);


  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_end), mainw->prv_link==PRV_PTR);


  add_hsep_to_box(LIVES_BOX(mainw->preview_controls));

  // buttons
  hbox_buttons = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), hbox_buttons, TRUE, TRUE, 0);

  rewind_img=lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  mainw->p_rewindbutton=lives_button_new();
  lives_widget_set_bg_color(mainw->p_rewindbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_button_set_relief(LIVES_BUTTON(mainw->p_rewindbutton), LIVES_RELIEF_NONE);
  lives_container_add(LIVES_CONTAINER(mainw->p_rewindbutton), rewind_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_rewindbutton, TRUE, TRUE, 0);
  lives_widget_show(mainw->p_rewindbutton);
  lives_widget_show(rewind_img);
  lives_widget_set_tooltip_text(mainw->p_rewindbutton,_("Rewind"));
  lives_widget_set_sensitive(mainw->p_rewindbutton, mainw->current_file>-1&&cfile->pointer_time>0.);

  play_img=lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  mainw->p_playbutton=lives_button_new();
  lives_widget_set_bg_color(mainw->p_playbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_button_set_relief(LIVES_BUTTON(mainw->p_playbutton), LIVES_RELIEF_NONE);
  lives_container_add(LIVES_CONTAINER(mainw->p_playbutton), play_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_playbutton, TRUE, TRUE, 0);
  lives_widget_show(mainw->p_playbutton);
  lives_widget_show(play_img);
  lives_widget_set_tooltip_text(mainw->p_playbutton,_("Play all"));

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"playsel.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  playsel_img=lives_image_new_from_file(buff);
  mainw->p_playselbutton=lives_button_new();
  lives_widget_set_bg_color(mainw->p_playselbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_button_set_relief(LIVES_BUTTON(mainw->p_playselbutton), LIVES_RELIEF_NONE);
  lives_container_add(LIVES_CONTAINER(mainw->p_playselbutton), playsel_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_playselbutton, TRUE, TRUE, 0);
  lives_widget_show(mainw->p_playselbutton);
  lives_widget_show(playsel_img);
  lives_widget_set_tooltip_text(mainw->p_playselbutton,_("Play Selection"));
  lives_widget_set_sensitive(mainw->p_playselbutton, mainw->current_file>-1&&cfile->frames>0);

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"loop.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  loop_img=lives_image_new_from_file(buff);
  mainw->p_loopbutton=lives_button_new();
  lives_widget_set_bg_color(mainw->p_loopbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_button_set_relief(LIVES_BUTTON(mainw->p_loopbutton), LIVES_RELIEF_NONE);
  lives_container_add(LIVES_CONTAINER(mainw->p_loopbutton), loop_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_loopbutton, TRUE, TRUE, 0);
  lives_widget_show(mainw->p_loopbutton);
  lives_widget_show(loop_img);
  lives_widget_set_tooltip_text(mainw->p_loopbutton,_("Loop On/Off"));
  lives_widget_set_sensitive(mainw->p_loopbutton, TRUE);

  fnamex=lives_build_filename(prefs->prefix_dir,ICON_DIR,"volume_mute.png",NULL);
  lives_snprintf(buff,PATH_MAX,"%s",fnamex);
  lives_free(fnamex);
  mainw->p_mute_img=lives_image_new_from_file(buff);
  if (lives_file_test(buff,LIVES_FILE_TEST_EXISTS)&&!mainw->mute) {
    LiVESPixbuf *pixbuf=lives_image_get_pixbuf(LIVES_IMAGE(mainw->p_mute_img));
    lives_pixbuf_saturate_and_pixelate(pixbuf,pixbuf,0.2,FALSE);
  }

  mainw->p_mutebutton=lives_button_new();
  lives_widget_set_bg_color(mainw->p_mutebutton, LIVES_WIDGET_STATE_ACTIVE, &palette->menu_and_bars);
  lives_button_set_relief(LIVES_BUTTON(mainw->p_mutebutton), LIVES_RELIEF_NONE);
  lives_container_add(LIVES_CONTAINER(mainw->p_mutebutton), mainw->p_mute_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_mutebutton, TRUE, TRUE, 0);
  lives_widget_show(mainw->p_mutebutton);
  lives_widget_show(mainw->p_mute_img);

  if (!mainw->mute) lives_widget_set_tooltip_text(mainw->p_mutebutton,_("Mute the audio (z)"));
  else lives_widget_set_tooltip_text(mainw->p_mutebutton,_("Unmute the audio (z)"));

  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_free), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prv_link_toggled),
                       LIVES_INT_TO_POINTER(PRV_FREE));
  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prv_link_toggled),
                       LIVES_INT_TO_POINTER(PRV_START));
  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_end), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prv_link_toggled),
                       LIVES_INT_TO_POINTER(PRV_END));
  lives_signal_connect(LIVES_GUI_OBJECT(radiobutton_ptr), LIVES_WIDGET_TOGGLED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_prv_link_toggled),
                       LIVES_INT_TO_POINTER(PRV_PTR));

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_playselbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playsel_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_rewindbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rewind_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_mutebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_mute_button_activate),
                       NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_loopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_loop_button_activate),
                       NULL);

  mainw->preview_spin_func=lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->preview_spinbutton), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_preview_spinbutton_changed),
                           NULL);

  lives_widget_show_all(mainw->preview_box);

}

#if GTK_CHECK_VERSION(3,0,0)

void calibrate_sepwin_size(void) {
  // get size of preview box in sepwin
  LiVESRequisition req;
  make_preview_box();
  lives_widget_get_preferred_size(mainw->preview_controls,NULL,&req);
  mainw->sepwin_minwidth=req.width;
  mainw->sepwin_minheight=req.height;
}

#endif

void enable_record(void) {
  set_menu_text(mainw->record_perf, _("Start _recording"),TRUE);
  lives_widget_set_sensitive(mainw->record_perf, TRUE);
}

void toggle_record(void) {
  set_menu_text(mainw->record_perf, _("Stop _recording"),TRUE);
}


void disable_record(void) {
  set_menu_text(mainw->record_perf, _("_Record Performance"),TRUE);
}



void play_window_set_title(void) {
  char *xtrabit;
  char *title;

  if (mainw->sepwin_scale!=100.) xtrabit=lives_strdup_printf(_(" (%d %% scale)"),(int)mainw->sepwin_scale);
  else xtrabit=lives_strdup("");

  if (mainw->playing_file>-1) {
    title=lives_strdup_printf(_("LiVES: - Play Window%s"),xtrabit);
    lives_window_set_title(LIVES_WINDOW(mainw->play_window), title);
  } else {
    title=lives_strdup_printf("%s%s",lives_window_get_title(LIVES_WINDOW
                              ((mainw->multitrack==NULL?mainw->LiVES:
                                mainw->multitrack->window))),
                              xtrabit);
    lives_window_set_title(LIVES_WINDOW(mainw->play_window),title);
  }
  lives_free(title);
  lives_free(xtrabit);
}


void resize_widgets_for_monitor(boolean get_play_times) {
  // resize widgets if we are aware that monitor resolution has changed

  mainw->scr_width=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].width;
  mainw->scr_height=mainw->mgeom[prefs->gui_monitor>0?prefs->gui_monitor-1:0].height;

  if (mainw->multitrack==NULL) {
    if (prefs->gui_monitor!=0) {
      lives_window_center(LIVES_WINDOW(mainw->LiVES));

    }
    if (prefs->open_maximised&&prefs->show_gui) {
      lives_window_maximize(LIVES_WINDOW(mainw->LiVES));
    }
  } else {
    if (prefs->gui_monitor!=0) {
      lives_window_center(LIVES_WINDOW(mainw->multitrack->window));
    }


    if ((prefs->gui_monitor!=0||capable->nmonitors<=1)&&prefs->open_maximised) {
      lives_window_maximize(LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  if (mainw->play_window!=NULL) {
    resize_play_window();
  }

  /*  if (mainw->multitrack==NULL&&get_play_times) {
    if (mainw->current_file>-1&&!mainw->recoverable_layout) {
      get_play_times();
    }
    }*/


}


void make_play_window(void) {
  //  separate window

  if (mainw->playing_file>-1) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

  lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->play_image),NULL);

  if (mainw->play_window!=NULL) {
    // this shouldn't ever happen
    kill_play_window();
  }

  mainw->play_window = lives_window_new(LIVES_WINDOW_TOPLEVEL);
  //  lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(mainw->LiVES),TRUE);

  lives_widget_set_events(mainw->play_window, LIVES_SCROLL_MASK);

  // cannot do this or it forces showing on the GUI monitor
  //gtk_window_set_position(LIVES_WINDOW(mainw->play_window),GTK_WIN_POS_CENTER_ALWAYS);

  if (mainw->multitrack==NULL) lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), mainw->accel_group);
  else lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), mainw->multitrack->accel_group);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(mainw->play_window, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  if (prefs->show_playwin) {
    // show the window (so we can hide its cursor !), and get its xwin
    if (!(mainw->fs&&mainw->playing_file>-1&&mainw->vpp!=NULL)) {
      lives_widget_show(mainw->play_window);
    }
  }

  resize_play_window();
  if (mainw->play_window==NULL) return;

  //if (mainw->playing_file==-1&&mainw->current_file>0&&cfile->frames>0&&mainw->multitrack==NULL) {
  if (mainw->multitrack==NULL&&mainw->playing_file==-1) {

    if (mainw->preview_box==NULL) {
      // create the preview box that shows frames
      make_preview_box();
      load_preview_image(FALSE);
    }

    //if (cfile->is_loaded) {
    //and add it the play window
    lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);

    if (mainw->is_processing&&mainw->current_file>-1&&!cfile->nopreview)
      lives_widget_set_tooltip_text(mainw->p_playbutton,_("Preview"));

    if (mainw->current_file>-1&&cfile->is_loaded) {
      lives_widget_grab_focus(mainw->preview_spinbutton);
    } else {
      lives_widget_hide(mainw->preview_controls);
    }

    if (mainw->playing_file>-1) {
      lives_widget_hide(mainw->preview_box);
    } else {
      if (mainw->is_processing&&(mainw->prv_link==PRV_START||mainw->prv_link==PRV_END)) {
        // block spinbutton in play window
        lives_widget_set_sensitive(mainw->preview_spinbutton,FALSE);
      }
    }
  }

  play_window_set_title();

  if ((mainw->current_file==-1||(!cfile->is_loaded&&!mainw->preview)||
       (cfile->frames==0&&(mainw->multitrack==NULL||mainw->playing_file==-1)))&&mainw->imframe!=NULL) {
    lives_painter_t *cr = lives_painter_create_from_widget(mainw->play_window);
    lives_painter_set_source_pixbuf(cr, mainw->imframe, (LiVESXModifierType)0, 0);
    lives_painter_paint(cr);
    lives_painter_destroy(cr);
  }

  lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Hide Play Window"));

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_DELETE_EVENT,
                       LIVES_GUI_CALLBACK(on_stop_activate_by_del),
                       NULL);


}



void resize_play_window(void) {
  int opwx,opwy,pmonitor=prefs->play_monitor,gmonitor=prefs->gui_monitor;

  boolean fullscreen=TRUE;
  boolean size_ok;

  int width=-1,height=-1,nwidth,nheight=0;

  uint64_t xwinid=0;

  mainw->sepwin_scale=100.;

#ifdef DEBUG_HANGS
  fullscreen=FALSE;
#endif

  if (mainw->play_window==NULL) return;

  if (lives_widget_is_visible(mainw->play_window)) {
    width=lives_widget_get_allocation_width(mainw->play_window);
    height=lives_widget_get_allocation_height(mainw->play_window);
  }

  if ((mainw->current_file==-1||(cfile->frames==0&&mainw->multitrack==NULL)||
       (!cfile->is_loaded&&!mainw->preview&&cfile->clip_type!=CLIP_TYPE_GENERATOR))||
      (mainw->multitrack!=NULL&&mainw->playing_file<1&&!mainw->preview)) {
    if (mainw->imframe!=NULL) {
      mainw->pwidth=lives_pixbuf_get_width(mainw->imframe);
      mainw->pheight=lives_pixbuf_get_height(mainw->imframe);
    } else {
      if (mainw->multitrack==NULL) {
        mainw->pwidth=DEFAULT_FRAME_HSIZE;
        mainw->pheight=DEFAULT_FRAME_VSIZE;
      }
    }
  } else {
    if (mainw->multitrack==NULL) {
      mainw->pwidth=cfile->hsize;
      mainw->pheight=cfile->vsize;
    } else {
      mainw->pwidth=mainw->files[mainw->multitrack->render_file]->hsize;
      mainw->pheight=mainw->files[mainw->multitrack->render_file]->vsize;
      mainw->must_resize=TRUE;
    }

    size_ok=FALSE;

    do {
      if (pmonitor==0) {
        if (mainw->pwidth>mainw->scr_width-SCR_WIDTH_SAFETY||
            mainw->pheight>mainw->scr_height-SCR_HEIGHT_SAFETY) {
          mainw->pheight=(mainw->pheight>>2)<<1;
          mainw->pwidth=(mainw->pwidth>>2)<<1;
          mainw->sepwin_scale/=2.;
        } else size_ok=TRUE;
      } else {
        if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-SCR_WIDTH_SAFETY||
            mainw->pheight>mainw->mgeom[pmonitor-1].height-SCR_HEIGHT_SAFETY) {
          mainw->pheight=(mainw->pheight>>2)<<1;
          mainw->pwidth=(mainw->pwidth>>2)<<1;
          mainw->sepwin_scale/=2.;
        } else size_ok=TRUE;
      }
    } while (!size_ok);
  }

  if (mainw->playing_file>-1) {
    if (mainw->double_size&&mainw->multitrack==NULL) {
      mainw->pheight*=2;
      mainw->pwidth*=2;
      if (pmonitor==0) {
        if (mainw->pwidth>mainw->scr_width-SCR_WIDTH_SAFETY||mainw->pheight>mainw->scr_height-SCR_HEIGHT_SAFETY) {
          calc_maxspect(mainw->scr_width-SCR_WIDTH_SAFETY,mainw->scr_height-SCR_HEIGHT_SAFETY,&mainw->pwidth,&mainw->pheight);
          mainw->sepwin_scale=(float)mainw->pwidth/(float)cfile->hsize*100.;
        }
      } else {
        if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-SCR_WIDTH_SAFETY||
            mainw->pheight>mainw->mgeom[pmonitor-1].height-SCR_HEIGHT_SAFETY) {
          calc_maxspect(mainw->mgeom[pmonitor-1].width-SCR_WIDTH_SAFETY,mainw->mgeom[pmonitor-1].height-SCR_HEIGHT_SAFETY,
                        &mainw->pwidth,&mainw->pheight);
          mainw->sepwin_scale=(float)mainw->pwidth/(float)cfile->hsize*100.;
        }
      }
    }

    if (mainw->fs) {
      if (!lives_widget_is_visible(mainw->play_window)) {
	if (prefs->show_playwin) {
	  lives_widget_show(mainw->play_window);
	}
        // be careful, the user could switch out of sepwin here !
        mainw->noswitch=TRUE;
        lives_widget_context_update();
        mainw->noswitch=FALSE;
        if (mainw->play_window==NULL) return;
        if (!mainw->fs||mainw->playing_file<0) goto point1;
        mainw->opwx=mainw->opwy=-1;
      } else {
        if (pmonitor==0) {
          mainw->opwx=(mainw->scr_width-mainw->pwidth)/2;
          mainw->opwy=(mainw->scr_height-mainw->pheight)/2;
        } else {
          mainw->opwx=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
          mainw->opwy=mainw->mgeom[pmonitor-1].y+(mainw->mgeom[pmonitor-1].height-mainw->pheight)/2;
        }
      }

      if (pmonitor==0) {
        mainw->pwidth=mainw->scr_width;
        mainw->pheight=mainw->scr_height;
        if (capable->nmonitors>1) {
          // spread over all monitors
          mainw->pwidth=lives_screen_get_width(mainw->mgeom[0].screen);
          mainw->pheight=lives_screen_get_height(mainw->mgeom[0].screen);
        }
      } else {
        mainw->pwidth=mainw->mgeom[pmonitor-1].width;
        mainw->pheight=mainw->mgeom[pmonitor-1].height;
      }

      if (lives_widget_is_visible(mainw->play_window)) {
        // store old postion of window
        lives_window_get_position(LIVES_WINDOW(mainw->play_window),&opwx,&opwy);
        if (opwx*opwy) {
          mainw->opwx=opwx;
          mainw->opwy=opwy;
        }
      }

      if (pmonitor==0) {
        if (mainw->vpp!=NULL&&mainw->vpp->fwidth>0) {
          lives_window_move(LIVES_WINDOW(mainw->play_window), (mainw->scr_width-mainw->vpp->fwidth)/2,
                            (mainw->scr_height-mainw->vpp->fheight)/2);
        } else lives_window_move(LIVES_WINDOW(mainw->play_window), 0, 0);
      } else {
        lives_window_set_screen(LIVES_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
        if (mainw->vpp!=NULL&&mainw->vpp->fwidth>0) {
          lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->mgeom[pmonitor-1].x+
                            (mainw->mgeom[pmonitor-1].width-mainw->vpp->fwidth)/2,
                            mainw->mgeom[pmonitor-1].y+(mainw->mgeom[pmonitor-1].height-mainw->vpp->fheight)/2);
        } else lives_window_move(LIVES_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].x,mainw->mgeom[pmonitor-1].y);
      }

      // leave this alone * !
      if (!(mainw->vpp!=NULL&&!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY))) {
        lives_window_fullscreen(LIVES_WINDOW(mainw->play_window));
        lives_window_resize(LIVES_WINDOW(mainw->play_window), mainw->pwidth, mainw->pheight);
        lives_widget_queue_resize(mainw->play_window);
      }

      // init the playback plugin, unless there is a possibility of wrongly sized frames (i.e. during a preview)
      if (mainw->vpp!=NULL&&(!mainw->preview||mainw->multitrack!=NULL)) {
        boolean fixed_size=FALSE;

        mainw->ptr_x=mainw->ptr_y=-1;
        if (pmonitor==0) {
          // fullscreen playback on all screens (of first display)
          // get mouse position to warp it back after playback ends
          // in future we will handle multiple displays, so we will get the mouse device for the first screen of that display
          LiVESXDevice *device=mainw->mgeom[0].mouse_device;
#if GTK_CHECK_VERSION(3,0,0)
          if (device!=NULL) {
#endif
            LiVESXScreen *screen;
            LiVESXDisplay *display=mainw->mgeom[0].disp;
            lives_display_get_pointer(device,display,&screen,&mainw->ptr_x,&mainw->ptr_y,NULL);
#if GTK_CHECK_VERSION(3,0,0)
          }
#endif
        }
        if (mainw->vpp->fheight>-1&&mainw->vpp->fwidth>-1) {
          // fixed o/p size for stream
          if (!(mainw->vpp->fwidth*mainw->vpp->fheight)) {
            if (mainw->current_file>-1) {
              mainw->vpp->fwidth=cfile->hsize;
              mainw->vpp->fheight=cfile->vsize;
            } else mainw->vpp->fwidth=mainw->vpp->fheight=-1;
          }
          mainw->pwidth=mainw->vpp->fwidth;
          mainw->pheight=mainw->vpp->fheight;
          fixed_size=TRUE;

          // * leave this alone !
          lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));

          if (!(mainw->vpp->capabilities&VPP_LOCAL_DISPLAY))
            lives_window_set_title(LIVES_WINDOW(mainw->play_window),_("LiVES: - Streaming"));

          lives_window_resize(LIVES_WINDOW(mainw->play_window), mainw->pwidth, mainw->pheight);
          lives_widget_queue_resize(mainw->play_window);
        }

        if (pmonitor!=0) {
          fullscreen=FALSE;
          if (mainw->play_window!=NULL) {
	    if (prefs->show_playwin) {
	      xwinid=lives_widget_get_xwinid(mainw->play_window,"Unsupported display type for playback plugin");
	      if (xwinid==-1) return;
	    }
	    else xwinid=-1;
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
            lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window),FALSE);
        }

#ifdef RT_AUDIO
        if (mainw->vpp->audio_codec!=AUDIO_CODEC_NONE&&prefs->stream_audio_out) {
          start_audio_stream();
        } else {
          clear_audio_stream();
        }
#endif

        if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY&&pmonitor==0)
          lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window),TRUE);

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

#define TEST_CE_THUMBS 0
      if (TEST_CE_THUMBS||(prefs->show_gui&&prefs->ce_thumb_mode&&prefs->play_monitor!=prefs->gui_monitor&&
                           prefs->play_monitor!=0&&
                           capable->nmonitors>1&&mainw->multitrack==NULL)) {
        start_ce_thumb_mode();
      }
    } else {

      if (mainw->ce_thumbs) {
        end_ce_thumb_mode();
      }

point1:
      if (mainw->playing_file==0) {
        mainw->pheight=clipboard->vsize;
        mainw->pwidth=clipboard->hsize;

        size_ok=FALSE;

        do {
          if (pmonitor==0) {
            if (mainw->pwidth>mainw->scr_width-SCR_WIDTH_SAFETY||
                mainw->pheight>mainw->scr_height-SCR_HEIGHT_SAFETY) {
              mainw->pheight=(mainw->pheight>>2)<<1;
              mainw->pwidth=(mainw->pwidth>>2)<<1;
            } else size_ok=TRUE;
          } else {
            if (mainw->pwidth>mainw->mgeom[pmonitor-1].width-SCR_WIDTH_SAFETY||
                mainw->pheight>mainw->mgeom[pmonitor-1].height-SCR_HEIGHT_SAFETY) {
              mainw->pheight=(mainw->pheight>>2)<<1;
              mainw->pwidth=(mainw->pwidth>>2)<<1;
            } else size_ok=TRUE;
          }
        } while (!size_ok);
      }
      if (pmonitor==0) lives_window_move(LIVES_WINDOW(mainw->play_window), (mainw->scr_width-mainw->pwidth)/2,
                                           (mainw->scr_height-mainw->pheight)/2);
      else {
        int xcen=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
        int ycen=mainw->mgeom[pmonitor-1].y+(mainw->mgeom[pmonitor-1].height-mainw->pheight)/2;
        lives_window_set_screen(LIVES_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
        lives_window_move(LIVES_WINDOW(mainw->play_window), xcen, ycen);
      }
    }
    if (prefs->show_playwin) {
      lives_window_present(LIVES_WINDOW(mainw->play_window));
      lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
    }
  } else {
    // not playing
    if (mainw->fs&&mainw->playing_file==-2&&mainw->sep_win&&prefs->sepwin_type==SEPWIN_TYPE_STICKY) {

      if (mainw->ce_thumbs) {
        end_ce_thumb_mode();
      }

      if (mainw->opwx>=0&&mainw->opwy>=0) {
        // move window back to its old position after play
        if (pmonitor>0) lives_window_set_screen(LIVES_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
        lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->opwx, mainw->opwy);
      } else {
        if (pmonitor==0) lives_window_move(LIVES_WINDOW(mainw->play_window), (mainw->scr_width-mainw->pwidth)/2,
                                             (mainw->scr_height-mainw->pheight-mainw->sepwin_minheight*2)/2);
        else {
          int xcen=mainw->mgeom[pmonitor-1].x+(mainw->mgeom[pmonitor-1].width-mainw->pwidth)/2;
          lives_window_set_screen(LIVES_WINDOW(mainw->play_window),mainw->mgeom[pmonitor-1].screen);
          lives_window_move(LIVES_WINDOW(mainw->play_window), xcen, (mainw->mgeom[pmonitor-1].height-mainw->pheight-mainw->sepwin_minheight*2)/2);
        }
      }
    } else {
      if (gmonitor==0) lives_window_move(LIVES_WINDOW(mainw->play_window), (mainw->scr_width-mainw->pwidth)/2,
                                           (mainw->scr_height-mainw->pheight-mainw->sepwin_minheight*2)/2);
      else {
        int xcen=mainw->mgeom[gmonitor-1].x+(mainw->mgeom[gmonitor-1].width-mainw->pwidth)/2;
        lives_window_set_screen(LIVES_WINDOW(mainw->play_window),mainw->mgeom[gmonitor-1].screen);
        lives_window_move(LIVES_WINDOW(mainw->play_window), xcen, (mainw->mgeom[gmonitor-1].height-mainw->pheight-mainw->sepwin_minheight*2)/2);
      }
    }
    mainw->opwx=mainw->opwy=-1;
  }
  if (mainw->playing_file<0&&(mainw->current_file>-1&&!cfile->opening)) nheight=mainw->sepwin_minheight;

  if (mainw->pheight<MIN_SEPWIN_HEIGHT) nheight+=MIN_SEPWIN_HEIGHT-mainw->pheight;

  nheight+=mainw->pheight;

  if (mainw->playing_file==-1&&mainw->current_file>-1&&cfile->frames>0&&
      (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE))
    nwidth=MAX(mainw->pwidth,mainw->sepwin_minwidth);
  else nwidth=mainw->pwidth;

  lives_window_resize(LIVES_WINDOW(mainw->play_window), nwidth, nheight);
  lives_widget_set_size_request(mainw->play_window, nwidth, nheight);

  if (width!=-1&&(width!=nwidth||height!=nheight)&&mainw->preview_spinbutton!=NULL)
    if (mainw->playing_file==-1) {
      load_preview_image(FALSE);
    }
}



void kill_play_window(void) {
  // plug our player back into internal window

  if (mainw->ce_thumbs) {
    end_ce_thumb_mode();
  }

  if (mainw->play_window!=NULL) {
    if (mainw->preview_box!=NULL&&lives_widget_get_parent(mainw->preview_box)!=NULL) {
      // preview_box is refed, so it will survive
      lives_container_remove(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
    }
    if (LIVES_IS_WINDOW(mainw->play_window)) lives_widget_destroy(mainw->play_window);
    mainw->play_window=NULL;
  }
  lives_widget_set_tooltip_text(mainw->m_sepwinbutton,_("Show Play Window"));
}



void add_to_playframe(void) {
  // plug the playback image into its frame in the main window

  lives_widget_show(mainw->playarea);

  ///////////////////////////////////////////////////
  if (mainw->plug==NULL) {
    if (!mainw->foreign&&(!mainw->sep_win||prefs->sepwin_type==SEPWIN_TYPE_NON_STICKY)) {
      mainw->plug = lives_hbox_new(FALSE,0);
      lives_container_add(LIVES_CONTAINER(mainw->playarea),mainw->plug);
      lives_widget_set_bg_color(mainw->plug, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
      lives_widget_show(mainw->plug);
      lives_container_add(LIVES_CONTAINER(mainw->plug), mainw->play_image);
    }
  }
}


LIVES_INLINE void frame_size_update(void) {
  // update widgets when the frame size changes
  on_double_size_activate(NULL,LIVES_INT_TO_POINTER(1));
}


char *get_menu_name(lives_clip_t *sfile) {
  if (sfile==NULL) return NULL;
  return sfile->clip_type!=CLIP_TYPE_VIDEODEV?lives_path_get_basename(sfile->name):lives_strdup(sfile->name);
}


void add_to_clipmenu(void) {
  // TODO - indicate "opening"
  char *tmp;

  cfile->menuentry = lives_radio_menu_item_new_with_label(mainw->clips_group, tmp=get_menu_name(cfile));
  lives_free(tmp);

  mainw->clips_group=lives_radio_menu_item_get_group(LIVES_RADIO_MENU_ITEM(cfile->menuentry));

  lives_widget_show(cfile->menuentry);
  lives_container_add(LIVES_CONTAINER(mainw->clipsmenu), cfile->menuentry);

  lives_widget_set_sensitive(cfile->menuentry, TRUE);
  cfile->menuentry_func=lives_signal_connect(LIVES_GUI_OBJECT(cfile->menuentry), LIVES_WIDGET_TOGGLED_SIGNAL,
                        LIVES_GUI_CALLBACK(switch_clip_activate),
                        NULL);

  if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) mainw->clips_available++;
  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_append(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  cfile->old_frames=cfile->frames;
  cfile->ratio_fps=check_for_ratio_fps(cfile->fps);
}




void remove_from_clipmenu(void) {
  LiVESList *list;
  int fileno;

  lives_container_remove(LIVES_CONTAINER(mainw->clipsmenu), cfile->menuentry);
  if (LIVES_IS_WIDGET(cfile->menuentry))
    lives_widget_destroy(cfile->menuentry);
  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist=lives_list_remove(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  if (cfile->clip_type==CLIP_TYPE_DISK||cfile->clip_type==CLIP_TYPE_FILE) {
    mainw->clips_available--;
    if (prefs->crash_recovery) rewrite_recovery_file();
  }

  list=mainw->cliplist;
  mainw->clips_group=NULL;
  while (list!=NULL) {
    fileno=LIVES_POINTER_TO_INT(list->data);
    if (mainw->files[fileno]!=NULL&&mainw->files[fileno]->menuentry!=NULL) {
      mainw->clips_group=lives_radio_menu_item_get_group(LIVES_RADIO_MENU_ITEM(mainw->files[fileno]->menuentry));
      break;
    }
  }

}





//////////////////////////////////////////////////////////////////////

// splash screen



void splash_init(void) {
  LiVESWidget *vbox,*hbox;
  LiVESWidget *splash_img;
  LiVESPixbuf *splash_pix;

  LiVESError *error=NULL;
  char *tmp=lives_strdup_printf("%s/%s/lives-splash.png",prefs->prefix_dir,THEME_DIR);

  lives_window_set_auto_startup_notification(FALSE);

  mainw->splash_window = lives_window_new(LIVES_WINDOW_TOPLEVEL);

  widget_opts.default_justify=LIVES_JUSTIFY_LEFT;

#ifdef GUI_GTK
  if (gtk_widget_get_direction(GTK_WIDGET(mainw->splash_window))==GTK_TEXT_DIR_RTL)
    widget_opts.default_justify=LIVES_JUSTIFY_RIGHT;
#endif

  if (prefs->show_splash) {

#ifdef GUI_GTK
    gtk_window_set_type_hint(LIVES_WINDOW(mainw->splash_window),GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
#endif

    if (palette->style&STYLE_1) {
      lives_widget_set_bg_color(mainw->splash_window, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }


    vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
    lives_container_add(LIVES_CONTAINER(mainw->splash_window), vbox);

    splash_pix=lives_pixbuf_new_from_file(tmp,&error);
    lives_free(tmp);

    splash_img = lives_image_new_from_pixbuf(splash_pix);

    lives_box_pack_start(LIVES_BOX(vbox), splash_img, TRUE, TRUE, 0);

    if (splash_pix!=NULL) lives_object_unref(splash_pix);


    mainw->splash_label=lives_standard_label_new("");

    lives_box_pack_start(LIVES_BOX(vbox), mainw->splash_label, TRUE, TRUE, 0);

    mainw->splash_progress = lives_progress_bar_new();
    lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(mainw->splash_progress),.01);

    if (palette->style&STYLE_1) {
      lives_widget_set_fg_color(mainw->splash_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_fg_color(mainw->splash_progress, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    }

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width);

    lives_box_pack_start(LIVES_BOX(hbox), mainw->splash_progress, TRUE, TRUE, widget_opts.packing_width*2);

    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height*2);

    lives_widget_show_all(mainw->splash_window);


    if (prefs->gui_monitor>0) {
      lives_window_set_screen(LIVES_WINDOW(mainw->splash_window),mainw->mgeom[prefs->gui_monitor-1].screen);
    }

    lives_window_center(LIVES_WINDOW(mainw->splash_window));

    lives_window_present(LIVES_WINDOW(mainw->splash_window));

    lives_widget_context_update();
    lives_set_cursor_style(LIVES_CURSOR_BUSY,mainw->splash_window);
  } else {
    lives_widget_destroy(mainw->splash_window);
    mainw->splash_window=NULL;
  }

  lives_window_set_auto_startup_notification(TRUE);

}




void splash_msg(const char *msg, double pct) {

  if (mainw->foreign||mainw->splash_window==NULL) return;

  lives_label_set_text(LIVES_LABEL(mainw->splash_label),msg);

  lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(mainw->splash_progress),pct);

  lives_widget_queue_draw(mainw->splash_window);

  lives_widget_context_update();

}





void splash_end(void) {

  if (mainw->foreign) return;

  if (mainw->splash_window!=NULL) {
    LiVESWidget *splash_window=mainw->splash_window; // end_threaded_dialog() will set mainw->splash_window to NULL

    end_threaded_dialog();

    lives_widget_destroy(splash_window);

  }

  if (prefs->startup_interface==STARTUP_MT&&prefs->startup_phase==0&&mainw->multitrack==NULL)
    on_multitrack_activate(NULL,NULL);

}
