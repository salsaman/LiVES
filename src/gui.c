// gui.c
// LiVES
// (c) G. Finch 2004 - 2021 <salsaman+lives@gmail.com>
// Released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// code for drawing the main window

#include "main.h"
#include "callbacks.h"
#include "interface.h"
#include "effects.h"
#include "rfx-builder.h"
#include "paramwindow.h"
#include "resample.h"
#include "rte_window.h"
#include "stream.h"
#include "startup.h"
#include "ce_thumbs.h"

#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef HAVE_YUV4MPEG
#include "lives-yuv4mpeg.h"
#endif

#ifdef HAVE_UNICAP
#include "videodev.h"
#endif

#ifdef HAVE_LDVGRAB
#include "ldvgrab.h"
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

static void _resize_play_window(void);

static boolean pb_added = FALSE;

LIVES_GLOBAL_INLINE int get_vspace(void) {
  static int vspace = -1;
  if (vspace == -1) {
    LiVESPixbuf *sepbuf = lives_image_get_pixbuf(LIVES_IMAGE(mainw->sep_image));
    vspace = (sepbuf ? lives_pixbuf_get_height(sepbuf) : 0);
  }
  return vspace;
}


void load_theme_images(void) {
  // load the theme images
  // TODO - set palette in here ?
  LiVESError *error = NULL;
  LiVESPixbuf *pixbuf;

  int width, height;

  pixbuf = lives_pixbuf_new_from_file(mainw->sepimg_path, &error);

  if (mainw->imsep) lives_widget_object_unref(mainw->imsep);
  mainw->imsep = NULL;
  if (mainw->imframe) lives_widget_object_unref(mainw->imframe);
  mainw->imframe = NULL;

  if (error) {
    palette->style = STYLE_PLAIN;
    LIVES_ERROR("Theme Error !");
    lives_snprintf(prefs->theme, 64, "%%ERROR%%");
    lives_error_free(error);
  } else {
    if (pixbuf) {
      //resize
      width = lives_pixbuf_get_width(pixbuf);
      height = lives_pixbuf_get_height(pixbuf);
      if (width > IMSEP_MAX_WIDTH || height > IMSEP_MAX_HEIGHT)
        calc_maxspect(IMSEP_MAX_WIDTH, IMSEP_MAX_HEIGHT,
                      &width, &height);
      if (prefs->screen_scale < 1.) {
        width *= prefs->screen_scale;
        height *= prefs->screen_scale;
      }
      mainw->imsep = lives_pixbuf_scale_simple(pixbuf, width, height, LIVES_INTERP_BEST);
      lives_widget_object_unref(pixbuf);
    }

    lives_image_set_from_pixbuf(LIVES_IMAGE(mainw->sep_image), mainw->imsep);
    // imframe

    pixbuf = lives_pixbuf_new_from_file(mainw->frameblank_path, &error);

    if (error) {
      lives_error_free(error);
    } else {
      if (pixbuf) {
        width = lives_pixbuf_get_width(pixbuf);
        height = lives_pixbuf_get_height(pixbuf);

        if (width < FRAMEBLANK_MIN_WIDTH) width = FRAMEBLANK_MIN_WIDTH;
        if (width > FRAMEBLANK_MAX_WIDTH) width = FRAMEBLANK_MAX_WIDTH;
        if (height < FRAMEBLANK_MIN_HEIGHT) height = FRAMEBLANK_MIN_HEIGHT;
        if (height > FRAMEBLANK_MAX_HEIGHT) height = FRAMEBLANK_MAX_HEIGHT;

        mainw->imframe = lives_pixbuf_scale_simple(pixbuf, width, height, LIVES_INTERP_BEST);
        lives_widget_object_unref(pixbuf);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


void make_custom_submenus(void) {
  mainw->custom_effects_submenu = lives_standard_menu_item_new_with_label(_("_Custom Effects"));
  lives_widget_set_no_show_all(mainw->custom_effects_submenu, TRUE);
  mainw->custom_gens_submenu = lives_standard_menu_item_new_with_label(_("_Custom Generators"));
  lives_widget_set_no_show_all(mainw->custom_gens_submenu, TRUE);
  mainw->custom_utilities_submenu = lives_standard_menu_item_new_with_label(_("Custom _Utilities"));
  lives_widget_set_no_show_all(mainw->custom_utilities_submenu, TRUE);
}


#if GTK_CHECK_VERSION(3, 0, 0)
boolean expose_sim(LiVESWidget * widget, lives_painter_t *cr, livespointer user_data) {
  if (mainw->is_generating) return TRUE;
  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((widget_opts.monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1) && (!mainw->ext_playback ||
                                            (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return TRUE;
  lives_painter_set_source_surface(cr, mainw->si_surface, 0., 0.);
  lives_painter_paint(cr);
  return FALSE;
}

boolean expose_eim(LiVESWidget * widget, lives_painter_t *cr, livespointer user_data) {
  if (mainw->is_generating) return TRUE;
  if (LIVES_IS_PLAYING && mainw->fs && (!mainw->sep_win || ((widget_opts.monitor == prefs->play_monitor ||
                                        capable->nmonitors == 1)  && (!mainw->ext_playback ||
                                            (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))))) return TRUE;
  lives_painter_set_source_surface(cr, mainw->ei_surface, 0., 0.);
  lives_painter_paint(cr);
  return FALSE;
}

boolean expose_pim(LiVESWidget * widget, lives_painter_t *cr, livespointer user_data) {
  //if (LIVES_IS_PLAYING) return FALSE;
  if (mainw->is_generating) return TRUE;
  lives_painter_set_source_surface(cr, mainw->pi_surface, 0., 0.);
  lives_painter_paint(cr);
  return FALSE;
}

#endif


void set_colours(LiVESWidgetColor * colf, LiVESWidgetColor * colb, LiVESWidgetColor * colf2,
                 LiVESWidgetColor * colb2, LiVESWidgetColor * colt, LiVESWidgetColor * coli) {
  LiVESWidget *label;
  lives_widget_apply_theme(LIVES_MAIN_WINDOW_WIDGET, LIVES_WIDGET_STATE_NORMAL);

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme2(mainw->menubar, LIVES_WIDGET_STATE_NORMAL, FALSE);
    lives_widget_apply_theme2(mainw->menu_hbox, LIVES_WIDGET_STATE_NORMAL, FALSE);
    lives_widget_apply_theme(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->freventbox0, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->frame1, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->freventbox1, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->frame2, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->pf_grid, LIVES_WIDGET_STATE_NORMAL);

    lives_widget_apply_theme(mainw->start_image, LIVES_WIDGET_STATE_NORMAL);
    lives_widget_apply_theme(mainw->end_image, LIVES_WIDGET_STATE_NORMAL);
  }

  lives_widget_set_bg_color(mainw->sa_hbox, LIVES_WIDGET_STATE_NORMAL, colb);

  if (palette->style & STYLE_1) {
    lives_widget_apply_theme2(mainw->vol_toolitem, LIVES_WIDGET_STATE_NORMAL, FALSE);
    lives_widget_apply_theme2(mainw->volume_scale, LIVES_WIDGET_STATE_NORMAL, FALSE);
  }

  if (mainw->plug) lives_widget_set_bg_color(mainw->plug, LIVES_WIDGET_STATE_NORMAL, colb);

  lives_widget_set_bg_color(mainw->sel_label, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->vidbar, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->laudbar, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->raudbar, LIVES_WIDGET_STATE_NORMAL, colb);

  if (palette->style & STYLE_1) {
    set_submenu_colours(LIVES_MENU(mainw->files_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->edit_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->play_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->effects_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->tools_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->audio_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->info_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->clipsmenu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->advanced_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->vj_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->toys_menu), colf2, colb2);
    set_submenu_colours(LIVES_MENU(mainw->help_menu), colf2, colb2);

    lives_widget_apply_theme2(mainw->l2_tb, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme2(mainw->l3_tb, LIVES_WIDGET_STATE_NORMAL, TRUE);
    lives_widget_apply_theme2(mainw->l2_tb, LIVES_WIDGET_STATE_INSENSITIVE, TRUE);
    lives_widget_apply_theme2(mainw->l3_tb, LIVES_WIDGET_STATE_INSENSITIVE, TRUE);

    lives_widget_apply_theme2(mainw->btoolbar, LIVES_WIDGET_STATE_NORMAL, TRUE);
#if GTK_CHECK_VERSION(3, 16, 0)
    if (mainw->pretty_colours) {
      char *colref2 = gdk_rgba_to_string(&palette->menu_and_bars);
      char *colref = gdk_rgba_to_string(&palette->normal_back);
      char *tmp = lives_strdup_printf("linear-gradient(%s, %s)", colref2, colref);
      int mh = 8;

      set_css_value_direct(mainw->btoolbar, LIVES_WIDGET_STATE_NORMAL, "", "background-image", tmp);
      set_css_value_direct(mainw->volume_scale, LIVES_WIDGET_STATE_NORMAL, "", "background-image", tmp);
      tmp = lives_strdup_printf("%dpx", mh);

      lives_free(colref); lives_free(colref2);

      lives_widget_set_size_request(mainw->volume_scale, -1, 12);

      set_css_value_direct(mainw->int_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, "*", "min-height", tmp);
      set_css_value_direct(mainw->ext_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, "*", "min-height", tmp);

      set_css_value_direct(mainw->int_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, "button", "min-height", tmp);
      set_css_value_direct(mainw->ext_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, "button", "min-height", tmp);
      lives_free(tmp);

      set_css_value_direct(mainw->l2_tb, LIVES_WIDGET_STATE_NORMAL, "", "opacity", "0.4");
      set_css_value_direct(mainw->l2_tb, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");

      set_css_value_direct(mainw->l3_tb, LIVES_WIDGET_STATE_NORMAL, "", "opacity", "0.4");
      set_css_value_direct(mainw->l3_tb, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");

      set_css_value_direct(lives_widget_get_parent(mainw->l2_tb), LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(lives_widget_get_parent(mainw->l2_tb)),
                           LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(mainw->l2_tb), LIVES_WIDGET_STATE_NORMAL, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(lives_widget_get_parent(mainw->l2_tb)),
                           LIVES_WIDGET_STATE_NORMAL, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(mainw->l3_tb), LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(lives_widget_get_parent(mainw->l3_tb)),
                           LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(mainw->l3_tb), LIVES_WIDGET_STATE_NORMAL, "", "opacity", "1.0");
      set_css_value_direct(lives_widget_get_parent(lives_widget_get_parent(mainw->l3_tb)),
                           LIVES_WIDGET_STATE_NORMAL, "", "opacity", "1.0");

      lives_widget_set_valign(mainw->int_audio_checkbutton, LIVES_ALIGN_START);
      lives_widget_set_valign(mainw->ext_audio_checkbutton, LIVES_ALIGN_START);

      lives_widget_set_valign(mainw->l1_tb, LIVES_ALIGN_START);
      lives_widget_set_valign(mainw->l2_tb, LIVES_ALIGN_START);
      lives_widget_set_valign(mainw->l3_tb, LIVES_ALIGN_START);
      lives_widget_set_valign(mainw->volume_scale, LIVES_ALIGN_START);

      gtk_button_set_image_position(LIVES_BUTTON(mainw->volume_scale), LIVES_POS_TOP);
      set_css_value_direct(mainw->vol_toolitem,  LIVES_WIDGET_STATE_NORMAL, "", "box-shadow", "none");
    }
#endif
    lives_widget_set_fg_color(mainw->l2_tb, LIVES_WIDGET_STATE_NORMAL, colf2);
    lives_widget_set_fg_color(mainw->l3_tb, LIVES_WIDGET_STATE_NORMAL, colf2);
    lives_widget_set_fg_color(mainw->l2_tb, LIVES_WIDGET_STATE_INSENSITIVE, colf2);
    lives_widget_set_fg_color(mainw->l3_tb, LIVES_WIDGET_STATE_INSENSITIVE, colf2);

    lives_widget_set_bg_color(mainw->eventbox, LIVES_WIDGET_STATE_NORMAL, colb);

    lives_widget_set_base_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame1)),
                                LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_text_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame1)),
                                LIVES_WIDGET_STATE_NORMAL, colf);

    lives_widget_set_bg_color(mainw->pl_eventbox, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_bg_color(mainw->play_image, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_bg_color(mainw->freventbox1, LIVES_WIDGET_STATE_NORMAL, colb);

    lives_widget_set_bg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_fg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, colb);
  }
  lives_widget_set_bg_color(mainw->top_vbox, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_apply_theme(mainw->eventbox2, LIVES_WIDGET_STATE_NORMAL);
  if (mainw->eventbox5) lives_widget_set_bg_color(mainw->eventbox5,
        LIVES_WIDGET_STATE_NORMAL, colb);

  /// no theme !
  lives_widget_set_bg_color(mainw->hruler, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_fg_color(mainw->hruler, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_bg_color(mainw->pf_grid, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->eventbox3, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->eventbox4, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->frame1, LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->frame2, LIVES_WIDGET_STATE_NORMAL, colb);

  lives_widget_set_fg_color(mainw->vidbar, LIVES_WIDGET_STATE_NORMAL, colf);
  lives_widget_set_fg_color(mainw->laudbar, LIVES_WIDGET_STATE_NORMAL, colf);
  lives_widget_set_fg_color(mainw->raudbar, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_apply_theme(mainw->video_draw, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme(mainw->laudio_draw, LIVES_WIDGET_STATE_NORMAL);
  lives_widget_apply_theme(mainw->raudio_draw, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_apply_theme(mainw->vps_label, LIVES_WIDGET_STATE_NORMAL);

  lives_widget_set_fg_color(mainw->banner, LIVES_WIDGET_STATE_NORMAL, colf);
  lives_widget_set_fg_color(mainw->arrow1, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_fg_color(mainw->sel_label, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_fg_color(mainw->arrow2, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_bg_color(mainw->playframe, LIVES_WIDGET_STATE_NORMAL, colb);

  label = lives_frame_get_label_widget(LIVES_FRAME(mainw->playframe));
  if (label) {
    lives_widget_set_base_color(label, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_text_color(label, LIVES_WIDGET_STATE_NORMAL, colf);
  }

  lives_widget_set_text_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame1)), LIVES_WIDGET_STATE_NORMAL, colf);
  lives_widget_set_text_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame2)), LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_base_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame2)), LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_base_color(lives_frame_get_label_widget(LIVES_FRAME(mainw->frame2)), LIVES_WIDGET_STATE_NORMAL, colb);

  if (prefs->show_msg_area) {
    lives_widget_set_fg_color(mainw->message_box, LIVES_WIDGET_STATE_NORMAL, colf);
    lives_widget_set_bg_color(mainw->msg_scrollbar, LIVES_WIDGET_STATE_NORMAL, colb2);
  }

  lives_widget_set_fg_color(mainw->pf_grid, LIVES_WIDGET_STATE_NORMAL, colb);

  lives_widget_set_bg_color(lives_widget_get_parent(mainw->framebar), LIVES_WIDGET_STATE_NORMAL, colb);
  lives_widget_set_bg_color(mainw->framebar, LIVES_WIDGET_STATE_NORMAL, colb);

  if (palette->style & STYLE_2) {
    lives_widget_set_base_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_base_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, colb);
    lives_widget_set_base_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, colb);
    lives_widget_set_base_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, colb);
    lives_widget_set_text_color(mainw->spinbutton_start, LIVES_WIDGET_STATE_NORMAL, colf);
    lives_widget_set_text_color(mainw->spinbutton_end, LIVES_WIDGET_STATE_NORMAL, colf);
  }

  lives_widget_set_fg_color(mainw->sel_label, LIVES_WIDGET_STATE_NORMAL, colf);

  lives_widget_set_bg_color(mainw->tb_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
  lives_widget_set_bg_color(mainw->toolbar, LIVES_WIDGET_STATE_NORMAL, &palette->fade_colour);
}


void create_LiVES(void) {
#ifdef GUI_GTK
#if !GTK_CHECK_VERSION(3, 0, 0)
  LiVESWidget *alignment;
#endif
#endif
  LiVESWidget *vbox;
  LiVESWidget *hbox;
  LiVESWidget *vbox2;
  LiVESWidget *menuitem;
  LiVESWidget *select_submenu_menu;
  LiVESWidget *submenu_menu;
  LiVESWidget *export_submenu_menu;
  LiVESWidget *trimaudio_submenu_menu;
  LiVESWidget *delaudio_submenu_menu;
  LiVESWidget *menuitemsep;
#if LIVES_HAS_IMAGE_MENU_ITEM
  LiVESWidget *image;
#endif
  LiVESWidget *rfx_menu;
  LiVESWidget *midi_menu;
  LiVESWidget *midi_submenu;
  LiVESWidget *midi_load;
  LiVESWidget *win;
  LiVESWidget *about;
  LiVESWidget *show_manual;
  LiVESWidget *email_author;
  LiVESWidget *donate;
  LiVESWidget *report_bug;
  LiVESWidget *suggest_feature;
  LiVESWidget *help_translate;
  LiVESWidget *vbox4;
  LiVESWidget *vbox99;
  LiVESWidget *label;
  LiVESWidget *hseparator;
  LiVESWidget *t_label;

#if defined HAVE_YUV4MPEG || defined HAVE_UNICAP
  LiVESWidget *submenu;
#endif

  LiVESWidget *new_test_rfx;
  LiVESWidget *copy_rfx;
  LiVESWidget *import_custom_rfx;
  LiVESWidget *rebuild_rfx;
  LiVESWidget *assign_rte_keys;

  LiVESWidget *tmp_toolbar_icon;

  LiVESAdjustment *adj;

  char buff[32768];

  char *tmp;

  int i;
  int dpw;
  int woat;
  boolean new_lives = FALSE;

  mainw->configured = FALSE;
  mainw->assumed_width = mainw->assumed_height = -1;

  stop_closure = fullscreen_closure = dblsize_closure = sepwin_closure = loop_closure = NULL;
  loop_cont_closure = fade_closure = showfct_closure = showsubs_closure = NULL;
  rec_closure = mute_audio_closure = ping_pong_closure = NULL;

  if (!LIVES_MAIN_WINDOW_WIDGET) {
    new_lives = TRUE;
    LIVES_MAIN_WINDOW_WIDGET = lives_window_new(LIVES_WINDOW_TOPLEVEL);
    lives_container_set_border_width(LIVES_CONTAINER(LIVES_MAIN_WINDOW_WIDGET), 0);
    lives_window_set_monitor(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), widget_opts.monitor);
    lives_widget_add_events(LIVES_MAIN_WINDOW_WIDGET, LIVES_KEY_PRESS_MASK | LIVES_KEY_RELEASE_MASK);
    mainw->config_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(LIVES_MAIN_WINDOW_WIDGET),
                         LIVES_WIDGET_CONFIGURE_EVENT, LIVES_GUI_CALLBACK(config_event), NULL);
  }
  ////////////////////////////////////

  mainw->double_size = mainw->sep_win = FALSE;

  mainw->current_file = -1;

  if (!mainw->preview_box) mainw->preview_image = NULL;
  mainw->sep_image = lives_image_new_from_pixbuf(NULL);
  mainw->imframe = mainw->imsep = NULL;

  mainw->start_image = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(expose_sim), &mainw->si_surface);
  mainw->end_image = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(expose_eim), &mainw->ei_surface);

  lives_widget_show(mainw->start_image);  // needed to get size
  lives_widget_show(mainw->end_image);  // needed to get size

  if (palette->style & STYLE_1) {
    load_theme_images();
  } else {
#ifdef GUI_GTK
    LiVESWidgetColor normal;
#if !GTK_CHECK_VERSION(3, 0, 0)
    if (!mainw->foreign) {
      gtk_widget_ensure_style(LIVES_MAIN_WINDOW_WIDGET);
    }
#endif
#endif

    lives_widget_get_bg_state_color(LIVES_MAIN_WINDOW_WIDGET, LIVES_WIDGET_STATE_NORMAL, &normal);
    lives_widget_color_copy((LiVESWidgetColor *)(&palette->normal_back), &normal);

    lives_widget_get_fg_color(LIVES_MAIN_WINDOW_WIDGET, &normal);
    lives_widget_color_copy((LiVESWidgetColor *)(&palette->normal_fore), &normal);
  }

  if (!new_lives) {
    lives_window_remove_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->accel_group);
  }
  mainw->accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());

  mainw->layout_textbuffer = lives_text_buffer_new();
  lives_widget_object_ref(mainw->layout_textbuffer);
  mainw->affected_layouts_map = NULL;

  lives_window_set_hide_titlebar_when_maximized(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), TRUE);

  if (new_lives) {
#ifdef GUI_GTK
    // TODO - can we use just DEFAULT_DROP ?
    gtk_drag_dest_set(LIVES_MAIN_WINDOW_WIDGET, GTK_DEST_DEFAULT_ALL, mainw->target_table, 2,
                      (GdkDragAction)(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK));

    lives_signal_connect(LIVES_GUI_OBJECT(LIVES_MAIN_WINDOW_WIDGET), LIVES_WIDGET_DRAG_DATA_RECEIVED_SIGNAL,
                         LIVES_GUI_CALLBACK(drag_from_outside), NULL);
#endif
    lives_window_set_decorated(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), prefs->open_decorated);
  }
  mainw->top_vbox = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(LIVES_MAIN_WINDOW_WIDGET), mainw->top_vbox);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->top_vbox), 0);

  //lives_widget_set_valign(mainw->top_vbox, LIVES_ALIGN_FILL);

  // top_vbox contains the following:
  // - menu_hbox -> menubar -> menuitems
  //
  // - tb_hbox -> toolbar -> buttons etc
  //
  // - eventbox (allows scrollwheel clip selection)
  //       - vbox99
  //            - vbox4
  //                  - framebar  (could probably become a direct child of top_vbox)
  //                        - fps spin
  //                        - banner (not always shown)
  //                        - framecounter
  //
  //                  - pf_grid
  //                      (- alignment : only for gtk+ 2.x)
  //                           - eventbox3 (to allow context menu clicks)
  //                                 - frame1 (could probably become a direct child of pf_grid)
  //                                       - freventbox1
  //                                             - start_image
  //                           - playframe
  //                                 - pl_eventbox
  //                                       - playarea           --> can be reparented to multitrack play_box (or anywhere)
  //                                             - plug
  //                                                    - play_image
  //                           - eventbox4 (ditto)
  //                                 - frame2  (ditto)
  //                                       - freventbox2
  //                                             - end_image
  //                  - hbox3
  //                          - start_spinbutton
  //                          - vbox
  //                                 - hbox
  //                                      - arrow1
  //                                      - sel_label
  //                                      - arrow2
  //                                 - sa_hbox
  //                                            - sa_button
  //                          - end_spinbutton
  //                  - sepimg / hseparator
  //
  //               - hruler (timeline)
  // - eventbox2 (mouse clicks etc)
  //      - vidbar
  //           - video_draw
  //                  - video_drawable (cairo surface)
  //      - laudbar
  //           - laudio_draw
  //                  - laudio_drawable (cairo surface)
  //      - raudbar
  //           - raudio_draw
  //                  - raudio_drawable (cairo surface)
  // - message_box
  //      - msg_area
  //      - msg_scrollbar

  mainw->menu_hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->menu_hbox, FALSE, FALSE, 0);
  lives_widget_set_valign(mainw->menu_hbox, LIVES_ALIGN_START);

  if (!widget_opts.apply_theme) {
    hseparator = lives_hseparator_new();
    lives_box_pack_start(LIVES_BOX(mainw->top_vbox), hseparator, FALSE, FALSE, 0);
  }

  mainw->menubar = lives_menu_bar_new();
  lives_box_pack_start(LIVES_BOX(mainw->menu_hbox), mainw->menubar, FALSE, FALSE, 0);

  menuitem = lives_standard_menu_item_new_with_label(_("_File"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->files_menu = lives_menu_new();
  lives_menu_set_accel_group(LIVES_MENU(mainw->files_menu), mainw->accel_group);

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->files_menu);

  mainw->open = lives_standard_menu_item_new_with_label(_("_Open File/Directory"));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->open);
  lives_widget_add_accelerator(mainw->open, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_o, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->open_sel = lives_standard_menu_item_new_with_label(_("O_pen Part of File..."));

  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->open_sel);

  mainw->open_loc = lives_standard_menu_item_new_with_label(_("Play Remote _Stream..."));

  mainw->open_loc_menu = lives_standard_menu_item_new_with_label(_("Open _Online Clip..."));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->open_loc_menu);

  mainw->open_loc_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_loc_menu), mainw->open_loc_submenu);

  mainw->open_utube = lives_standard_menu_item_new_with_label(_("Download from _Youtube or other site..."));
  lives_container_add(LIVES_CONTAINER(mainw->open_loc_submenu), mainw->open_utube);

  lives_container_add(LIVES_CONTAINER(mainw->open_loc_submenu), mainw->open_loc);

  mainw->open_vcd_menu = lives_standard_menu_item_new_with_label(_("Import from _dvd/vcd..."));

  // TODO: show these options, but give errors for no mplayer / mplayer2
  // TODO - mpv
#ifdef ENABLE_DVD_GRAB
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->open_vcd_menu);
#endif

  mainw->open_vcd_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_vcd_menu), mainw->open_vcd_submenu);

  mainw->open_dvd = lives_standard_menu_item_new_with_label(_("Import from _dvd"));
  mainw->open_vcd = lives_standard_menu_item_new_with_label(_("Import from _vcd"));

  lives_container_add(LIVES_CONTAINER(mainw->open_vcd_submenu), mainw->open_dvd);
  lives_container_add(LIVES_CONTAINER(mainw->open_vcd_submenu), mainw->open_vcd);

  mainw->open_device_menu = lives_standard_menu_item_new_with_label(_("_Import from Firewire"));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->open_device_menu);
  mainw->open_device_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->open_device_menu), mainw->open_device_submenu);

  mainw->open_firewire = lives_standard_menu_item_new_with_label(_("Import from _Firewire Device (dv)"));
  mainw->open_hfirewire = lives_standard_menu_item_new_with_label(_("Import from _Firewire Device (hdv)"));

#ifdef HAVE_LDVGRAB
  lives_container_add(LIVES_CONTAINER(mainw->open_device_submenu), mainw->open_firewire);
  lives_container_add(LIVES_CONTAINER(mainw->open_device_submenu), mainw->open_hfirewire);
#endif

  menuitem = lives_standard_menu_item_new_with_label(_("_Add Webcam/TV card..."));
  mainw->unicap = lives_standard_menu_item_new_with_label(_("Add Webcam"));
  mainw->firewire = lives_standard_menu_item_new_with_label(_("Add Live _Firewire Device"));
  mainw->tvdev = lives_standard_menu_item_new_with_label(_("Add _TV Device"));

#if defined(HAVE_UNICAP) || defined(HAVE_YUV4MPEG)
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), menuitem);

  submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), submenu);

#ifdef HAVE_UNICAP
  lives_container_add(LIVES_CONTAINER(submenu), mainw->unicap);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->unicap), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vdev_activate), NULL);
#endif

#ifdef HAVE_YUV4MPEG
  // TODO: mpv
  if (capable->has_dvgrab) {
    if (HAS_EXTERNAL_PLAYER && !USE_MPV) {
      lives_container_add(LIVES_CONTAINER(submenu), mainw->firewire);

      lives_signal_connect(LIVES_GUI_OBJECT(mainw->firewire), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(on_live_fw_activate), NULL);
    }

    lives_container_add(LIVES_CONTAINER(submenu), mainw->tvdev);

    lives_signal_connect(LIVES_GUI_OBJECT(mainw->tvdev), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_live_tvcard_activate), NULL);
  }

#endif
#endif // defined HAVE_UNICAP || defined HAVE_YUV4MPEG

  mainw->recent_menu = lives_standard_menu_item_new_with_label(_("_Recent Files..."));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->recent_menu);
  mainw->recent_submenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->recent_menu), mainw->recent_submenu);

  buff[0] = 0;
  widget_opts.mnemonic_label = FALSE;
  for (i = 0; i < N_RECENT_FILES; i++) {
    char *prefname = lives_strdup_printf("%s%d", PREF_RECENT, i + 1);
    get_utf8_pref(prefname, buff, PATH_MAX * 2);
    lives_free(prefname);
    for (register int j = 0; buff[j]; j++) {
      if (buff[j] == '\n') {
        buff[j] = 0;
        break;
      }
    }
    mainw->recent[i] = lives_standard_menu_item_new_with_label(buff);
    lives_widget_set_no_show_all(mainw->recent[i], TRUE);
    lives_container_add(LIVES_CONTAINER(mainw->recent_submenu), mainw->recent[i]);
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->recent[i]), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_recent_activate), LIVES_INT_TO_POINTER(i + 1));
  }
  widget_opts.mnemonic_label = TRUE;

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));

  mainw->vj_load_set = lives_standard_menu_item_new_with_label(_("_Reload Clip Set..."));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->vj_load_set);

  mainw->vj_save_set = lives_standard_menu_item_new_with_label(_("Close/Sa_ve All Clips"));
  lives_widget_set_sensitive(mainw->vj_save_set, FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->vj_save_set);

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));

#ifdef LIBAV_TRANSCODE
  mainw->transcode = lives_standard_menu_item_new_with_label(_("_Quick Transcode..."));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->transcode);
  lives_widget_set_sensitive(mainw->transcode, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));
#endif

  mainw->save_as = lives_standard_image_menu_item_new_from_stock(LIVES_STOCK_LABEL_SAVE, mainw->accel_group);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->save_as);
  lives_widget_set_sensitive(mainw->save_as, FALSE);
  lives_menu_item_set_text(mainw->save_as, _("_Encode Clip As..."), TRUE);

  mainw->save_selection = lives_standard_menu_item_new_with_label(_("Encode _Selection As..."));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->save_selection);
  lives_widget_set_sensitive(mainw->save_selection, FALSE);

  mainw->close = lives_standard_menu_item_new_with_label(_("_Close This Clip"));
  lives_widget_add_accelerator(mainw->close, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_w, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->close);
  lives_widget_set_sensitive(mainw->close, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));

  mainw->backup = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("_Backup Clip as .%s..."),
                  LIVES_FILE_EXT_BACKUP)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->backup);
  lives_widget_set_sensitive(mainw->backup, FALSE);

  lives_widget_add_accelerator(mainw->backup, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_b, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->restore = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("_Restore Clip from .%s..."),
                   LIVES_FILE_EXT_BACKUP)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->restore);

  lives_widget_add_accelerator(mainw->restore, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_r, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));

  mainw->sw_sound = lives_standard_check_menu_item_new_with_label(_("Encode/Load/Backup _with Sound"), TRUE);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->sw_sound);

  if (prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->sw_sound, FALSE);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->sw_sound), FALSE);
  }

  mainw->aload_subs = lives_standard_check_menu_item_new_with_label(_("Auto load subtitles"), prefs->autoload_subs);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->aload_subs);

  lives_menu_add_separator(LIVES_MENU(mainw->files_menu));

  mainw->clear_ds = lives_standard_menu_item_new_with_label(_("Clean _up Diskspace / Recover Missing Clips"));
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->clear_ds);

  mainw->quit = lives_standard_image_menu_item_new_from_stock(LIVES_STOCK_LABEL_QUIT, mainw->accel_group);
  lives_container_add(LIVES_CONTAINER(mainw->files_menu), mainw->quit);

  menuitem = lives_standard_menu_item_new_with_label(_("_Edit"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->edit_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->edit_menu);

  mainw->undo = lives_standard_image_menu_item_new_with_label(_("_Undo"));

  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->undo);
  lives_widget_set_sensitive(mainw->undo, FALSE);

  lives_widget_add_accelerator(mainw->undo, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_u, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_UNDO, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->undo), image);
#endif

  mainw->redo = lives_standard_image_menu_item_new_with_label(_("_Redo"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->redo);
  lives_widget_set_sensitive(mainw->redo, FALSE);

  lives_widget_add_accelerator(mainw->redo, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_z, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REDO, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->redo), image);
#endif

  lives_menu_add_separator(LIVES_MENU(mainw->edit_menu));

  mainw->mt_menu = lives_standard_image_menu_item_new_with_label(_("_MULTITRACK mode"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->mt_menu);

  lives_menu_add_separator(LIVES_MENU(mainw->edit_menu));

  lives_widget_add_accelerator(mainw->mt_menu, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_m, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->copy = lives_standard_image_menu_item_new_with_label(_("_Copy Selection"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->copy);
  lives_widget_set_sensitive(mainw->copy, FALSE);

  lives_widget_add_accelerator(mainw->copy, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_c, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->cut = lives_standard_image_menu_item_new_with_label(_("Cu_t Selection"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->cut);
  lives_widget_set_sensitive(mainw->cut, FALSE);

  lives_widget_add_accelerator(mainw->cut, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_t, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->insert = lives_standard_image_menu_item_new_with_label(_("_Insert from Clipboard..."));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->insert);
  lives_widget_set_sensitive(mainw->insert, FALSE);

  lives_widget_add_accelerator(mainw->insert, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_i, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_ADD, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->insert), image);
#endif

  mainw->paste_as_new = lives_standard_image_menu_item_new_with_label(_("Paste as _New"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->paste_as_new);
  lives_widget_set_sensitive(mainw->paste_as_new, FALSE);

  lives_widget_add_accelerator(mainw->paste_as_new, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_n, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->merge = lives_standard_menu_item_new_with_label(_("_Merge Clipboard with Selection..."));

  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->merge);
  lives_widget_set_sensitive(mainw->merge, FALSE);

  mainw->xdelete = lives_standard_image_menu_item_new_with_label(_("_Delete Selection"));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->xdelete);
  lives_widget_set_sensitive(mainw->xdelete, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_DELETE, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->xdelete), image);
#endif

  lives_widget_add_accelerator(mainw->xdelete, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_d, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mainw->edit_menu));

  mainw->ccpd_sound = lives_standard_check_menu_item_new_with_label(_("Decouple _Video from Audio"), !mainw->ccpd_with_sound);
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->ccpd_sound);

  if (prefs->vj_mode) {
    lives_widget_set_sensitive(mainw->ccpd_sound, FALSE);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->ccpd_sound), TRUE);
  }

  lives_menu_add_separator(LIVES_MENU(mainw->edit_menu));

  mainw->select_submenu = lives_standard_menu_item_new_with_label(_("_Select..."));
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->select_submenu);

  select_submenu_menu = lives_menu_new();

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->select_submenu), select_submenu_menu);
  lives_widget_set_sensitive(mainw->select_submenu, FALSE);

  mainw->select_all = lives_standard_menu_item_new_with_label(_("Select _All Frames"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_all);

  lives_widget_add_accelerator(mainw->select_all, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_a, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->select_start_only = lives_standard_image_menu_item_new_with_label(_("_Start Frame Only"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_start_only);

  lives_widget_add_accelerator(mainw->select_start_only, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_Home, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->select_end_only = lives_standard_image_menu_item_new_with_label(_("_End Frame Only"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_end_only);
  lives_widget_add_accelerator(mainw->select_end_only, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_End, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(select_submenu_menu));

  mainw->select_from_start = lives_standard_image_menu_item_new_with_label(_("Select from _First Frame"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_from_start);

  mainw->select_to_end = lives_standard_image_menu_item_new_with_label(_("Select to _Last Frame"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_to_end);

  mainw->select_to_aend = lives_standard_image_menu_item_new_with_label(_("Select to _Audio End"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_to_aend);

  mainw->select_new = lives_standard_image_menu_item_new_with_label(_("Select Last Insertion/_Merge"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_new);

  mainw->select_last = lives_standard_image_menu_item_new_with_label(_("Select Last _Effect"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_last);

  mainw->select_invert = lives_standard_image_menu_item_new_with_label(_("_Invert Selection"));
  lives_container_add(LIVES_CONTAINER(select_submenu_menu), mainw->select_invert);

  lives_widget_add_accelerator(mainw->select_invert, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_Slash, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->lock_selwidth = lives_standard_check_menu_item_new_with_label(_("_Lock Selection Width"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->edit_menu), mainw->lock_selwidth);
  lives_widget_set_sensitive(mainw->lock_selwidth, FALSE);

  menuitem = lives_standard_menu_item_new_with_label(_("_Play"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->play_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->play_menu);

  mainw->playall = lives_standard_image_menu_item_new_with_label(_("_Play All"));
  lives_widget_add_accelerator(mainw->playall, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_p, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->playall);
  lives_widget_set_sensitive(mainw->playall, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_REFRESH, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playall), image);
#endif

  mainw->playsel = lives_standard_image_menu_item_new_with_label(_("Pla_y Selection"));
  lives_widget_add_accelerator(mainw->playsel, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_y, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->playsel);
  lives_widget_set_sensitive(mainw->playsel, FALSE);

  mainw->playclip = lives_standard_image_menu_item_new_with_label(_("Play _Clipboard"));
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->playclip);
  lives_widget_set_sensitive(mainw->playclip, FALSE);

  lives_widget_add_accelerator(mainw->playclip, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_c, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playsel), image);
#endif

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->playclip), image);
#endif

  mainw->stop = lives_standard_image_menu_item_new_with_label(_("_Stop"));
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->stop);
  lives_widget_set_sensitive(mainw->stop, FALSE);
  lives_widget_add_accelerator(mainw->stop, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_q, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->stop), image);
#endif

  mainw->rewind = lives_standard_image_menu_item_new_with_label(_("Re_wind"));
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->rewind);
  lives_widget_set_sensitive(mainw->rewind, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, LIVES_ICON_SIZE_MENU);

  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->rewind), image);
#endif

  lives_widget_add_accelerator(mainw->rewind, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_w, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->record_perf = lives_standard_check_menu_item_new_with_label("", FALSE);

  disable_record();

  lives_widget_add_accelerator(mainw->record_perf, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_r, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->record_perf);

  lives_menu_add_separator(LIVES_MENU(mainw->play_menu));

  mainw->full_screen = lives_standard_check_menu_item_new_with_label(_("_Full Screen"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->full_screen);

  lives_widget_add_accelerator(mainw->full_screen, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_f, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->dsize = lives_standard_check_menu_item_new_with_label(_("_Double Size"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->dsize);

  lives_widget_add_accelerator(mainw->dsize, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_d, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->sepwin = lives_standard_check_menu_item_new_with_label(_("Play in _Separate Window"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->sepwin);

  lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_s, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->fade = lives_standard_check_menu_item_new_with_label(_("_Blank Background"), FALSE);

  lives_widget_add_accelerator(mainw->fade, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_b, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->fade);

  mainw->loop_video = lives_standard_check_menu_item_new_with_label(_("Stop on _Audio End"), mainw->loop);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->loop_video);
  lives_widget_set_sensitive(mainw->loop_video, FALSE);
  lives_widget_add_accelerator(mainw->loop_video, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_l, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->loop_continue = lives_standard_check_menu_item_new_with_label(_("L_oop Continuously"), mainw->loop_cont);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->loop_continue);
  lives_widget_set_sensitive(mainw->loop_continue, FALSE);

  lives_widget_add_accelerator(mainw->loop_continue, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_o, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->loop_ping_pong = lives_standard_check_menu_item_new_with_label(_("Pin_g Pong Loops"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->loop_ping_pong);

  lives_widget_add_accelerator(mainw->loop_ping_pong, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_g, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->mute_audio = lives_standard_check_menu_item_new_with_label(_("_Mute"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->mute_audio);
  lives_widget_set_sensitive(mainw->mute_audio, FALSE);

  lives_widget_add_accelerator(mainw->mute_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_z, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_menu_add_separator(LIVES_MENU(mainw->play_menu));

  mainw->sticky = lives_standard_check_menu_item_new_with_label(_("Separate Window 'S_ticky' Mode"),
                  prefs->sepwin_type == SEPWIN_TYPE_STICKY);

  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->sticky);

  mainw->showfct = lives_standard_check_menu_item_new_with_label(_("S_how Frame Counter"), !prefs->hide_framebar);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->showfct);

  lives_widget_add_accelerator(mainw->showfct, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_h, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->showsubs = lives_standard_check_menu_item_new_with_label(_("Show Subtitles"), prefs->show_subtitles);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->showsubs);

  lives_widget_add_accelerator(mainw->showsubs, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_v, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  mainw->letter = lives_standard_check_menu_item_new_with_label(_("Letterbox Mode"), prefs->letterbox);
  lives_container_add(LIVES_CONTAINER(mainw->play_menu), mainw->letter);

  menuitem = lives_standard_menu_item_new_with_label(_("Effect_s"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);
  if (!prefs->vj_mode)
    lives_widget_set_tooltip_text(menuitem, (_("Effects are applied to the current selection.")));

  // the dynamic effects menu
  mainw->effects_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->effects_menu);

  mainw->custom_tools_submenu = lives_standard_menu_item_new_with_label(_("Custom _Tools"));
  lives_widget_set_no_show_all(mainw->custom_tools_submenu, TRUE);

  if (!prefs->vj_mode) {
    if (!RFX_LOADED) {
      mainw->ldg_menuitem = lives_standard_menu_item_new_with_label(_("Loading..."));
      lives_container_add(LIVES_CONTAINER(mainw->effects_menu), mainw->ldg_menuitem);
    }
  } else {
    mainw->ldg_menuitem = lives_standard_menu_item_new_with_label(_("Rendered effects disabled in VJ Mode"));
  }

  mainw->custom_effects_menu = NULL;
  mainw->custom_effects_separator = NULL;

  mainw->run_test_rfx_submenu = lives_standard_menu_item_new_with_label(_("_Run Test Rendered Effect/Tool/Generator..."));
  mainw->run_test_rfx_menu = NULL;

  mainw->num_rendered_effects_builtin = mainw->num_rendered_effects_custom = mainw->num_rendered_effects_test = 0;

  menuitem = lives_standard_menu_item_new_with_label(_("_Tools"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);
  if (!prefs->vj_mode)
    lives_widget_set_tooltip_text(menuitem, (_("Tools are applied to complete clips.")));

  mainw->tools_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->tools_menu);

  mainw->rev_clipboard = lives_standard_menu_item_new_with_label(_("_Reverse Clipboard"));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->rev_clipboard);
  lives_widget_set_sensitive(mainw->rev_clipboard, FALSE);

  lives_widget_add_accelerator(mainw->rev_clipboard, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_x, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  mainw->change_speed = lives_standard_menu_item_new_with_label(_("_Change Playback/Save Speed..."));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->change_speed);
  lives_widget_set_sensitive(mainw->change_speed, FALSE);

  mainw->resample_video = lives_standard_menu_item_new_with_label(_("Resample _Video to New Frame Rate..."));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->resample_video);
  lives_widget_set_sensitive(mainw->resample_video, FALSE);

  mainw->utilities_menu = NULL;
  mainw->utilities_submenu = lives_standard_menu_item_new_with_label(_("_Utilities"));

  mainw->custom_utilities_menu = NULL;

  mainw->custom_tools_separator = lives_standard_menu_item_new();
  lives_widget_set_sensitive(mainw->custom_tools_separator, FALSE);

  mainw->gens_menu = NULL;
  mainw->gens_submenu = lives_standard_menu_item_new_with_label(_("_Generate"));

  // add RFX plugins
  mainw->rte_separator = mainw->custom_gens_menu = mainw->custom_gens_submenu = NULL;
  mainw->custom_tools_menu = NULL;

  if (!mainw->foreign) {
    splash_msg(_("Starting GUI..."), SPLASH_LEVEL_START_GUI);
  }

  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->utilities_submenu);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->custom_tools_separator);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->custom_tools_submenu);
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->gens_submenu);

  lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));

  mainw->load_subs = lives_standard_menu_item_new_with_label(_("Load _Subtitles from File..."));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->load_subs);
  lives_widget_set_sensitive(mainw->load_subs, FALSE);

  mainw->erase_subs = lives_standard_menu_item_new_with_label(_("Erase subtitles"));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->erase_subs);
  lives_widget_set_sensitive(mainw->erase_subs, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));

  mainw->capture = lives_standard_menu_item_new_with_label(_("Capture _External Window... "));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->capture);

  lives_menu_add_separator(LIVES_MENU(mainw->tools_menu));

  mainw->preferences = lives_standard_image_menu_item_new_with_label(_("_Preferences..."));
  lives_container_add(LIVES_CONTAINER(mainw->tools_menu), mainw->preferences);
  lives_widget_add_accelerator(mainw->preferences, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_p, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_PREFERENCES, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->preferences), image);
#endif

  menuitem = lives_standard_menu_item_new_with_label(_("_Audio"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->audio_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->audio_menu);

  mainw->load_audio = lives_standard_menu_item_new_with_label(_("Load _New Audio for Clip..."));

  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->load_audio);
  lives_widget_set_sensitive(mainw->load_audio, FALSE);

  mainw->load_cdtrack = lives_standard_menu_item_new_with_label(_("Load _CD Track..."));
  mainw->eject_cd = lives_standard_menu_item_new_with_label(_("E_ject CD"));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->load_cdtrack);
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->eject_cd);

  if (!check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)
      && !check_for_executable(&capable->has_icedax, EXEC_ICEDAX)) {
    lives_widget_set_sensitive(mainw->load_cdtrack, FALSE);
    lives_widget_set_sensitive(mainw->eject_cd, FALSE);
  }

  mainw->recaudio_submenu = lives_standard_menu_item_new_with_label(_("Record E_xternal Audio..."));
  if ((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) || (prefs->audio_player == AUD_PLAYER_PULSE &&
      capable->has_pulse_audio))
    lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->recaudio_submenu);

  submenu_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->recaudio_submenu), submenu_menu);

  mainw->recaudio_clip = lives_standard_menu_item_new_with_label(_("to New _Clip..."));
  lives_container_add(LIVES_CONTAINER(submenu_menu), mainw->recaudio_clip);

  mainw->recaudio_sel = lives_standard_menu_item_new_with_label(_("to _Selection"));
  lives_container_add(LIVES_CONTAINER(submenu_menu), mainw->recaudio_sel);
  lives_widget_set_sensitive(mainw->recaudio_sel, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->audio_menu));

  mainw->voladj = lives_standard_menu_item_new_with_label(_("Change clip volume..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->voladj);

  mainw->fade_aud_in = lives_standard_menu_item_new_with_label(_("Fade Audio _In..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->fade_aud_in);

  mainw->fade_aud_out = lives_standard_menu_item_new_with_label(_("Fade Audio _Out..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->fade_aud_out);

  lives_widget_set_sensitive(mainw->voladj, FALSE);
  lives_widget_set_sensitive(mainw->fade_aud_in, FALSE);
  lives_widget_set_sensitive(mainw->fade_aud_out, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->audio_menu));

  mainw->export_submenu = lives_standard_menu_item_new_with_label(_("_Export Audio..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->export_submenu);

  export_submenu_menu = lives_menu_new();
  lives_widget_set_sensitive(export_submenu_menu, FALSE);

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->export_submenu), export_submenu_menu);
  lives_widget_set_sensitive(mainw->export_submenu, FALSE);

  mainw->export_selaudio = lives_standard_menu_item_new_with_label(_("Export _Selected Audio..."));
  lives_container_add(LIVES_CONTAINER(export_submenu_menu), mainw->export_selaudio);

  mainw->export_allaudio = lives_standard_menu_item_new_with_label(_("Export _All Audio..."));
  lives_container_add(LIVES_CONTAINER(export_submenu_menu), mainw->export_allaudio);

  mainw->append_audio = lives_standard_menu_item_new_with_label(_("_Append Audio..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->append_audio);
  lives_widget_set_sensitive(mainw->append_audio, FALSE);

  mainw->trim_submenu = lives_standard_menu_item_new_with_label(_("_Trim/Pad Audio..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->trim_submenu);

  trimaudio_submenu_menu = lives_menu_new();
  lives_widget_set_sensitive(trimaudio_submenu_menu, FALSE);

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->trim_submenu), trimaudio_submenu_menu);
  lives_widget_set_sensitive(mainw->trim_submenu, FALSE);

  mainw->trim_audio = lives_standard_menu_item_new_with_label(_("Trim/Pad Audio to _Selection"));
  lives_container_add(LIVES_CONTAINER(trimaudio_submenu_menu), mainw->trim_audio);

  mainw->trim_to_pstart = lives_standard_menu_item_new_with_label(_("Trim/Pad Audio from Beginning to _Play Start"));
  lives_container_add(LIVES_CONTAINER(trimaudio_submenu_menu), mainw->trim_to_pstart);

  mainw->delaudio_submenu = lives_standard_menu_item_new_with_label(_("_Delete Audio..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->delaudio_submenu);
  if (prefs->vj_mode) lives_widget_set_sensitive(mainw->delaudio_submenu, FALSE);

  delaudio_submenu_menu = lives_menu_new();

  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->delaudio_submenu), delaudio_submenu_menu);
  //lives_widget_set_sensitive(mainw->delaudio_submenu, FALSE);

  mainw->delsel_audio = lives_standard_menu_item_new_with_label(_("Delete _Selected Audio"));
  lives_container_add(LIVES_CONTAINER(delaudio_submenu_menu), mainw->delsel_audio);

  mainw->delall_audio = lives_standard_menu_item_new_with_label(_("Delete _All Audio"));
  lives_container_add(LIVES_CONTAINER(delaudio_submenu_menu), mainw->delall_audio);

  mainw->ins_silence = lives_standard_menu_item_new_with_label(_("Insert _Silence in Selection"));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->ins_silence);
  lives_widget_set_sensitive(mainw->ins_silence, FALSE);

  mainw->resample_audio = lives_standard_menu_item_new_with_label(_("_Resample Audio..."));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->resample_audio);
  lives_widget_set_sensitive(mainw->resample_audio, FALSE);

  mainw->normalize_audio = lives_standard_menu_item_new_with_label(_("_Normalize Audio"));
  lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->normalize_audio);
  lives_widget_set_sensitive(mainw->normalize_audio, FALSE);

  //mainw->adj_audio_sync = lives_standard_menu_item_new_with_label(_("_Adjust Audio Sync..."));
  //lives_container_add(LIVES_CONTAINER(mainw->audio_menu), mainw->adj_audio_sync);
  //lives_widget_set_sensitive(mainw->adj_audio_sync, FALSE);

  menuitem = lives_standard_menu_item_new_with_label(_("_Info"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->info_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->info_menu);

  mainw->show_file_info = lives_standard_image_menu_item_new_with_label(_("Show Clip _Info"));
  lives_widget_add_accelerator(mainw->show_file_info, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_i, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_file_info);
  lives_widget_set_sensitive(mainw->show_file_info, FALSE);

  mainw->show_file_comments = lives_standard_image_menu_item_new_with_label(_("Show/_Edit File Comments"));
  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_file_comments);
  lives_widget_set_sensitive(mainw->show_file_comments, FALSE);

  mainw->show_clipboard_info = lives_standard_image_menu_item_new_with_label(_("Show _Clipboard Info"));
  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_clipboard_info);
  lives_widget_set_sensitive(mainw->show_clipboard_info, FALSE);

#if LIVES_HAS_IMAGE_MENU_ITEM
  image = lives_image_new_from_stock(LIVES_STOCK_DIALOG_INFO, LIVES_ICON_SIZE_MENU);
  lives_image_menu_item_set_image(LIVES_IMAGE_MENU_ITEM(mainw->show_file_info), image);
#endif

  mainw->show_messages = lives_standard_image_menu_item_new_with_label(_("Show _Messages"));
  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_messages);

  mainw->show_layout_errors = lives_standard_image_menu_item_new_with_label(_("Show _Layout Errors"));
  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_layout_errors);
  lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);

  mainw->show_quota = lives_standard_image_menu_item_new_with_label(_("Show / Edit Disk _Quota Settings"));
  lives_container_add(LIVES_CONTAINER(mainw->info_menu), mainw->show_quota);

  win = lives_standard_menu_item_new_with_label(_("_Clips"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), win);

  mainw->clipsmenu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(win), mainw->clipsmenu);

  mainw->rename = lives_standard_image_menu_item_new_with_label(_("_Rename Current Clip in Menu..."));
  lives_container_add(LIVES_CONTAINER(mainw->clipsmenu), mainw->rename);
  lives_widget_set_sensitive(mainw->rename, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->clipsmenu));

  menuitemsep = lives_standard_menu_item_new_with_label("|");
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitemsep);
  lives_widget_set_sensitive(menuitemsep, FALSE);

  menuitem = lives_standard_menu_item_new_with_label(_("A_dvanced"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->advanced_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->advanced_menu);

  //mainw->optimize = lives_standard_menu_item_new_with_label(_("Check for Optimizations"));
  //lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->optimize);
  //lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->optimize), LIVES_WIDGET_ACTIVATE_SIGNAL,
  //			    LIVES_GUI_CALLBACK(optimize), NULL);

  //lives_menu_add_separator(LIVES_MENU(mainw->advanced_menu));

  mainw->rfx_submenu = lives_standard_menu_item_new_with_label(_("_RFX Effects/Tools/Utilities"));
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->rfx_submenu);

  if (prefs->vj_mode) lives_widget_set_sensitive(mainw->rfx_submenu, FALSE);
  if (mainw->helper_procthreads[PT_LAZY_RFX]) lives_widget_set_sensitive(mainw->rfx_submenu, FALSE);

  rfx_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->rfx_submenu), rfx_menu);

  new_test_rfx = lives_standard_menu_item_new_with_label(_("_New Test RFX Script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), new_test_rfx);

  copy_rfx = lives_standard_menu_item_new_with_label(_("_Copy RFX Script to Test..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), copy_rfx);

  mainw->edit_test_rfx = lives_standard_menu_item_new_with_label(_("_Edit Test RFX Script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->edit_test_rfx);

  mainw->rename_test_rfx = lives_standard_menu_item_new_with_label(_("Rena_me Test RFX Script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->rename_test_rfx);

  mainw->delete_test_rfx = lives_standard_menu_item_new_with_label(_("_Delete Test RFX Script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->delete_test_rfx);

  lives_menu_add_separator(LIVES_MENU(rfx_menu));

  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->run_test_rfx_submenu);

  mainw->promote_test_rfx = lives_standard_menu_item_new_with_label(_("_Promote Test Rendered Effect/Tool/Generator..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->promote_test_rfx);

  lives_menu_add_separator(LIVES_MENU(rfx_menu));

  import_custom_rfx = lives_standard_menu_item_new_with_label(_("_Import Custom RFX script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), import_custom_rfx);

  mainw->export_custom_rfx = lives_standard_menu_item_new_with_label(_("E_xport Custom RFX script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->export_custom_rfx);

  mainw->delete_custom_rfx = lives_standard_menu_item_new_with_label(_("De_lete Custom RFX Script..."));
  lives_container_add(LIVES_CONTAINER(rfx_menu), mainw->delete_custom_rfx);

  lives_menu_add_separator(LIVES_MENU(rfx_menu));

  rebuild_rfx = lives_standard_menu_item_new_with_label(_("Re_build test RFX plugins"));
  lives_container_add(LIVES_CONTAINER(rfx_menu), rebuild_rfx);

  mainw->open_lives2lives = lives_standard_menu_item_new_with_label(_("Receive _LiVES Stream from..."));
  lives_widget_set_sensitive(mainw->open_lives2lives, FALSE); // buggy for now

  lives_menu_add_separator(LIVES_MENU(mainw->advanced_menu));

  mainw->send_lives2lives = lives_standard_menu_item_new_with_label(_("_Send LiVES Stream to..."));
  lives_widget_set_sensitive(mainw->send_lives2lives, FALSE); // buggy for now

  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->send_lives2lives);
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->open_lives2lives);

  mainw->open_yuv4m = lives_standard_menu_item_new_with_label((tmp = (_("Open _yuv4mpeg stream..."))));
  lives_free(tmp);
#ifdef HAVE_YUV4MPEG
  lives_menu_add_separator(LIVES_MENU(mainw->advanced_menu));

  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->open_yuv4m);

  // TODO - apply a deinterlace filter to yuv4mpeg frames
  /*mainw->yuv4m_deint = lives_standard_check_menu_item_new_with_label (_("_Deinterlace yuv4mpeg frames"));
    lives_widget_show (mainw->yuv4m_deint);
    lives_container_add (LIVES_CONTAINER (advance_menu), mainw->yuv4m_deint);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->yu4m_deint),TRUE);*/
#endif

  lives_menu_add_separator(LIVES_MENU(mainw->advanced_menu));

  mainw->import_proj = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("_Import Project (.%s)..."),
                       LIVES_FILE_EXT_PROJECT)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->import_proj);

  mainw->export_proj = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("E_xport Project (.%s)..."),
                       LIVES_FILE_EXT_PROJECT)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->export_proj);
  lives_widget_set_sensitive(mainw->export_proj, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->advanced_menu));

  mainw->import_theme = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("_Import Custom Theme (.%s)..."),
                        LIVES_FILE_EXT_TAR_GZ)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->import_theme);

  mainw->export_theme = lives_standard_menu_item_new_with_label((tmp = lives_strdup_printf(_("E_xport Theme (.%s)..."),
                        LIVES_FILE_EXT_TAR_GZ)));
  lives_free(tmp);
  lives_container_add(LIVES_CONTAINER(mainw->advanced_menu), mainw->export_theme);
  lives_widget_set_sensitive(mainw->export_theme, (palette->style & STYLE_1));

  // VJ menu

  menuitem = lives_standard_menu_item_new_with_label(_("_VJ"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->vj_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->vj_menu);

  assign_rte_keys = lives_standard_menu_item_new_with_label(_("Real Time _Effect Mapping"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), assign_rte_keys);
  lives_widget_add_accelerator(assign_rte_keys, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                               LIVES_KEY_v, LIVES_CONTROL_MASK, LIVES_ACCEL_VISIBLE);

  lives_widget_set_tooltip_text(assign_rte_keys, (_("Bind real time effects to ctrl-number keys.")));

  mainw->rte_defs_menu = lives_standard_menu_item_new_with_label(_("Set Real Time Effect _Defaults"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->rte_defs_menu);
  lives_widget_set_tooltip_text(mainw->rte_defs_menu, (_("Set default parameter values for real time effects.")));

  mainw->rte_defs = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(mainw->rte_defs_menu), mainw->rte_defs);

  mainw->save_rte_defs = lives_standard_menu_item_new_with_label(_("Save Real Time Effect _Defaults"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->save_rte_defs);
  lives_widget_set_tooltip_text(mainw->save_rte_defs,
                                (_("Save real time effect defaults so they will be restored each time you use LiVES.")));

  lives_menu_add_separator(LIVES_MENU(mainw->vj_menu));

  mainw->vj_realize = lives_standard_menu_item_new_with_label(_("_Pre-decode all frames (unlocks reverse playback)"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->vj_realize);
  lives_widget_set_tooltip_text(mainw->vj_realize,
                                (_("Decode all frames to images. This will unlock reverse playback and can "
                                   "improve random seek times,\n"
                                   "but may require additional diskspace.")));
  lives_widget_set_sensitive(mainw->vj_realize, FALSE);

  mainw->vj_reset = lives_standard_menu_item_new_with_label(_("_Reset All Playback Speeds and Positions"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->vj_reset);
  lives_widget_set_tooltip_text(mainw->vj_reset,
                                (_("Reset all playback positions to frame 1, and reset all playback frame rates.")));
  lives_widget_set_sensitive(mainw->vj_reset, FALSE);

  midi_submenu = lives_standard_menu_item_new_with_label(_("_MIDI/Joystick Interface"));

#ifdef ENABLE_OSC
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), midi_submenu);
#endif

  midi_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(midi_submenu), midi_menu);

  mainw->midi_learn = lives_standard_menu_item_new_with_label(_("_MIDI/Joystick Learner..."));

  lives_container_add(LIVES_CONTAINER(midi_menu), mainw->midi_learn);

  mainw->midi_save = lives_standard_menu_item_new_with_label(_("_Save Device Mapping..."));

  lives_container_add(LIVES_CONTAINER(midi_menu), mainw->midi_save);
  lives_widget_set_sensitive(mainw->midi_save, FALSE);

  midi_load = lives_standard_menu_item_new_with_label(_("_Load Device Mapping..."));

  lives_container_add(LIVES_CONTAINER(midi_menu), midi_load);

  lives_menu_add_separator(LIVES_MENU(mainw->vj_menu));

  mainw->vj_show_keys = lives_standard_menu_item_new_with_label(_("Show VJ _Keys"));
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->vj_show_keys);

  lives_menu_add_separator(LIVES_MENU(mainw->vj_menu));

  mainw->vj_mode = lives_standard_check_menu_item_new_with_label(_("Restart in _VJ Mode"), future_prefs->vj_mode);
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->vj_mode);
  mainw->vj_mode_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->vj_mode), LIVES_WIDGET_ACTIVATE_SIGNAL,
                        LIVES_GUI_CALLBACK(vj_mode_toggled), NULL);

  lives_menu_add_separator(LIVES_MENU(mainw->vj_menu));

  mainw->autolives = lives_standard_check_menu_item_new_with_label(_("_Automatic Mode (autolives)..."), FALSE);
#ifdef ENABLE_OSC
  lives_container_add(LIVES_CONTAINER(mainw->vj_menu), mainw->autolives);
#endif

  menuitem = lives_standard_menu_item_new_with_label(_("To_ys"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->toys_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->toys_menu);

  mainw->toy_none = lives_standard_check_menu_item_new_with_label(mainw->string_constants[LIVES_STRING_CONSTANT_NONE], TRUE);
  lives_container_add(LIVES_CONTAINER(mainw->toys_menu), mainw->toy_none);

  mainw->toy_random_frames = lives_standard_check_menu_item_new_with_label(_("_Mad Frames"), FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->toys_menu), mainw->toy_random_frames);

  mainw->toy_tv = lives_standard_check_menu_item_new_with_label(_("_LiVES TV (broadband)"), FALSE);

  if (0 && !prefs->vj_mode)
    lives_container_add(LIVES_CONTAINER(mainw->toys_menu), mainw->toy_tv);

  menuitem = lives_standard_menu_item_new_with_label(_("_Help"));
  lives_container_add(LIVES_CONTAINER(mainw->menubar), menuitem);

  mainw->help_menu = lives_menu_new();
  lives_menu_item_set_submenu(LIVES_MENU_ITEM(menuitem), mainw->help_menu);

  show_manual = lives_standard_menu_item_new_with_label(_("_Manual (opens in browser)"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), show_manual);
  if (prefs->vj_mode) lives_widget_set_sensitive(show_manual, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->help_menu));

  donate = lives_standard_menu_item_new_with_label(_("_Donate to the Project !"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), donate);
  if (prefs->vj_mode) lives_widget_set_sensitive(donate, FALSE);

  email_author = lives_standard_menu_item_new_with_label(_("_Email the Author"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), email_author);
  if (prefs->vj_mode) lives_widget_set_sensitive(email_author, FALSE);

  report_bug = lives_standard_menu_item_new_with_label(_("Report a _Bug"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), report_bug);
  if (prefs->vj_mode) lives_widget_set_sensitive(report_bug, FALSE);

  suggest_feature = lives_standard_menu_item_new_with_label(_("Suggest a _Feature"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), suggest_feature);
  if (prefs->vj_mode) lives_widget_set_sensitive(suggest_feature, FALSE);

  help_translate = lives_standard_menu_item_new_with_label(_("Assist with _Translating"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), help_translate);
  if (prefs->vj_mode) lives_widget_set_sensitive(help_translate, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->help_menu));

  mainw->show_devopts = lives_standard_check_menu_item_new_with_label(_("Enable Developer Options"),
                        prefs->show_dev_opts);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->show_devopts),  prefs->show_dev_opts);

  lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->show_devopts);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_devopts), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(toggle_sets_pref), (livespointer)PREF_SHOW_DEVOPTS);

  mainw->dev_dabg = lives_standard_check_menu_item_new_for_var(_("Show drawing area backgrounds"),
                    &prefs->dev_show_dabg, FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->dev_dabg);
  menu_sets_visible(LIVES_CHECK_MENU_ITEM(mainw->show_devopts), mainw->dev_dabg, FALSE);

  mainw->dev_timing = lives_standard_check_menu_item_new_for_var(_("Show frame timings on console"),
                      &prefs->dev_show_timing, FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->dev_timing);
  menu_sets_visible(LIVES_CHECK_MENU_ITEM(mainw->show_devopts), mainw->dev_timing, FALSE);

  mainw->dev_caching = lives_standard_check_menu_item_new_for_var(_("Show cache predictions on console"),
                       &prefs->dev_show_caching, FALSE);
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->dev_caching);
  menu_sets_visible(LIVES_CHECK_MENU_ITEM(mainw->show_devopts), mainw->dev_caching, FALSE);

  lives_menu_add_separator(LIVES_MENU(mainw->help_menu));

  mainw->troubleshoot = lives_standard_menu_item_new_with_label(_("_Troubleshoot"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->troubleshoot);

  mainw->expl_missing = lives_standard_menu_item_new_with_label(_("Check for Optional Features"));
  if (!prefs->vj_mode) lives_container_add(LIVES_CONTAINER(mainw->help_menu), mainw->expl_missing);

  lives_menu_add_separator(LIVES_MENU(mainw->help_menu));

  about = lives_standard_menu_item_new_with_label(_("_About"));
  lives_container_add(LIVES_CONTAINER(mainw->help_menu), about);

  mainw->btoolbar = lives_standard_toolbar_new();

  ////

  lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->btoolbar, FALSE, TRUE, 0);
  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_SEPWIN,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_sepwinbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                          _("Show the play window (s)"));
  lives_widget_set_focus_on_click(mainw->m_sepwinbutton, FALSE);
  lives_widget_set_opacity(mainw->m_sepwinbutton, .75);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_rewindbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                          _("Rewind to start (w)"));

  lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
  lives_widget_set_focus_on_click(mainw->m_rewindbutton, FALSE);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_playbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                        _("Play all (p)"));
  lives_widget_set_sensitive(mainw->m_playbutton, FALSE);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_stopbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                        _("Stop playback (q)"));
  lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
  lives_widget_set_focus_on_click(mainw->m_stopbutton, FALSE);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_PLAY_SEL,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));
  mainw->m_playselbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                           _("Play selection (y)"));

  lives_widget_set_sensitive(mainw->m_playselbutton, FALSE);
  lives_widget_set_focus_on_click(mainw->m_playselbutton, FALSE);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_LOOP,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_loopbutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                        _("Switch continuous looping on (o)"));
  lives_widget_set_opacity(mainw->m_loopbutton, .75);
  lives_widget_set_focus_on_click(mainw->m_loopbutton, FALSE);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_VOLUME_MUTE,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->btoolbar)));

  mainw->m_mutebutton = lives_standard_tool_button_new(LIVES_TOOLBAR(mainw->btoolbar), LIVES_WIDGET(tmp_toolbar_icon), "",
                        _("Mute the audio (z)"));
  lives_widget_set_opacity(mainw->m_mutebutton, .75);
  lives_widget_set_focus_on_click(mainw->m_mutebutton, FALSE);

  for (i = 0; i < 3; i++) {
    lives_toolbar_insert_space(LIVES_TOOLBAR(mainw->btoolbar));
  }
  mainw->l1_tb = lives_toolbar_insert_label(LIVES_TOOLBAR(mainw->btoolbar), _("Audio Source:    "), NULL);
  lives_widget_set_valign(mainw->l1_tb, LIVES_ALIGN_START);

  widget_opts.expand = LIVES_EXPAND_NONE;
  lives_toolbar_insert_space(LIVES_TOOLBAR(mainw->btoolbar));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  mainw->int_audio_checkbutton = lives_toggle_tool_button_new();

#if GTK_CHECK_VERSION(3, 0, 0)
  // insert audio src buttons
  if (prefs->lamp_buttons) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->int_audio_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);

    lives_widget_set_bg_color(mainw->int_audio_checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(mainw->int_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->int_audio_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                              LIVES_GUI_CALLBACK(lives_cool_toggled), NULL);
    lives_cool_toggled(mainw->int_audio_checkbutton, NULL);
  }
#endif

  if (!mainw->int_audio_checkbutton) mainw->int_audio_checkbutton = lives_toggle_tool_button_new();

  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->int_audio_checkbutton), -1);

  woat = widget_opts.apply_theme;

  widget_opts.expand = LIVES_EXPAND_NONE;
  widget_opts.apply_theme = 0;
  lives_toolbar_insert_label(LIVES_TOOLBAR(mainw->btoolbar), _("  Internal"), mainw->int_audio_checkbutton);

  mainw->l2_tb = widget_opts.last_label;
  lives_widget_set_valign(mainw->l2_tb, LIVES_ALIGN_START);

  widget_opts.apply_theme = woat;
  lives_toolbar_insert_space(LIVES_TOOLBAR(mainw->btoolbar));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_widget_set_sensitive(mainw->l2_tb, prefs->audio_src != AUDIO_SRC_INT);
  lives_widget_set_sensitive(mainw->int_audio_checkbutton, prefs->audio_src != AUDIO_SRC_INT);
  lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->int_audio_checkbutton),
                                      prefs->audio_src == AUDIO_SRC_INT);

  toggle_toolbutton_sets_sensitive(LIVES_TOGGLE_TOOL_BUTTON(mainw->int_audio_checkbutton), mainw->l2_tb, TRUE);

  mainw->ext_audio_checkbutton = lives_toggle_tool_button_new();

  mainw->int_audio_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->int_audio_checkbutton),
                          LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(on_audio_toggled), NULL);

#if GTK_CHECK_VERSION(3, 0, 0)
  // insert audio src buttons
  if (prefs->lamp_buttons) {
    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->ext_audio_checkbutton), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);

    lives_widget_set_bg_color(mainw->ext_audio_checkbutton, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(mainw->ext_audio_checkbutton, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->ext_audio_checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                                    LIVES_GUI_CALLBACK(lives_cool_toggled), NULL);

    lives_cool_toggled(mainw->ext_audio_checkbutton, NULL);
  }
#endif

  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->ext_audio_checkbutton), -1);

  widget_opts.expand = LIVES_EXPAND_NONE;
  widget_opts.apply_theme = 0;
  lives_toolbar_insert_label(LIVES_TOOLBAR(mainw->btoolbar), _("  External"), mainw->ext_audio_checkbutton);
  widget_opts.apply_theme = woat;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  mainw->l3_tb = widget_opts.last_label;
  lives_widget_set_valign(mainw->l3_tb, LIVES_ALIGN_START);

  lives_widget_set_sensitive(mainw->l3_tb, prefs->audio_src != AUDIO_SRC_EXT);
  lives_widget_set_sensitive(mainw->ext_audio_checkbutton, prefs->audio_src != AUDIO_SRC_EXT);
  lives_toggle_tool_button_set_active(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_checkbutton),
                                      prefs->audio_src == AUDIO_SRC_EXT);

  toggle_toolbutton_sets_sensitive(LIVES_TOGGLE_TOOL_BUTTON(mainw->ext_audio_checkbutton), mainw->l3_tb, TRUE);

  mainw->ext_audio_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->ext_audio_checkbutton),
                          LIVES_WIDGET_TOGGLED_SIGNAL, LIVES_GUI_CALLBACK(on_audio_toggled), mainw->l3_tb);

  if (!is_realtime_aplayer(prefs->audio_player) || prefs->audio_player == AUD_PLAYER_NONE) {
    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
  }

#if GTK_CHECK_VERSION(3, 0, 0)
  // insert audio src buttons
  if (prefs->lamp_buttons) {
    mainw->ext_audio_mon = lives_toggle_tool_button_new();
    lives_widget_set_size_request(mainw->ext_audio_mon, 8, 8);

    lives_widget_set_sensitive(mainw->ext_audio_mon, FALSE);
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->ext_audio_mon), -1);
    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->ext_audio_mon), LIVES_WIDGET_EXPOSE_EVENT,
                              LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);

    lives_widget_set_bg_color(mainw->ext_audio_mon, LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
    lives_widget_set_bg_color(mainw->ext_audio_mon, LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);
    lives_cool_toggled(mainw->ext_audio_mon, NULL);
  }
#endif

#ifdef TEST_VOL_LIGHTS
#if GTK_CHECK_VERSION(3, 0, 0)
  widget_opts.expand = LIVES_EXPAND_NONE;
  lives_toolbar_insert_space(LIVES_TOOLBAR(mainw->btoolbar));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  for (i = 0; i < NUM_VOL_LIGHTS; i++) {
    // insert audio src buttons
    if (prefs->lamp_buttons) {
      mainw->vol_checkbuttons[i][0] = lives_toggle_tool_button_new();
      lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->vol_checkbuttons[i][0]), -1);

      lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->vol_checkbuttons[i][0]), LIVES_WIDGET_EXPOSE_EVENT,
                                LIVES_GUI_CALLBACK(draw_cool_toggle), NULL);

      lives_widget_set_bg_color(mainw->vol_checkbuttons[i][0], LIVES_WIDGET_STATE_ACTIVE, &palette->light_green);
      lives_widget_set_bg_color(mainw->vol_checkbuttons[i][0], LIVES_WIDGET_STATE_NORMAL, &palette->dark_red);

      lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->vol_checkbuttons[i][0]), LIVES_WIDGET_TOGGLED_SIGNAL,
                                      LIVES_GUI_CALLBACK(lives_cool_toggled), NULL);

      lives_cool_toggled(mainw->vol_checkbuttons[i][0], NULL);
    }
  }
#endif
#endif

  adj = lives_adjustment_new(prefs->volume, 0., 1., 0.01, 0.01, 0.);

  mainw->volume_scale = lives_volume_button_new(LIVES_ORIENTATION_HORIZONTAL, adj, prefs->volume);

  mainw->vol_label = NULL;

  if (LIVES_IS_RANGE(mainw->volume_scale)) {
    mainw->vol_label = LIVES_WIDGET(lives_tool_item_new());
    label = lives_label_new(_("Volume"));
    lives_container_add(LIVES_CONTAINER(mainw->vol_label), label);
    lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->vol_label), -1);
  } else lives_widget_object_unref(adj);

  mainw->vol_toolitem = LIVES_WIDGET(lives_tool_item_new());
  lives_container_set_border_width(LIVES_CONTAINER(mainw->vol_toolitem), 0);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->volume_scale), 0);

  lives_button_set_relief(LIVES_BUTTON(mainw->volume_scale), LIVES_RELIEF_NORMAL);

#ifdef GUI_GTK
  gtk_tool_item_set_homogeneous(LIVES_TOOL_ITEM(mainw->vol_toolitem), FALSE);
  gtk_tool_item_set_expand(LIVES_TOOL_ITEM(mainw->vol_toolitem), TRUE);
#endif

  lives_container_add(LIVES_CONTAINER(mainw->vol_toolitem), mainw->volume_scale);
  lives_toolbar_insert_space(LIVES_TOOLBAR(mainw->btoolbar));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->btoolbar), LIVES_TOOL_ITEM(mainw->vol_toolitem), -1);

  lives_widget_set_tooltip_text(mainw->vol_toolitem, _("Audio volume (1.00)"));

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->volume_scale), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_volume_slider_value_changed), NULL);

  mainw->tb_hbox = lives_hbox_new(FALSE, 0);
  mainw->toolbar = lives_toolbar_new();
  lives_widget_set_no_show_all(mainw->tb_hbox, TRUE);

  lives_toolbar_set_show_arrow(LIVES_TOOLBAR(mainw->toolbar), FALSE);

  lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->tb_hbox, FALSE, FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->tb_hbox), mainw->toolbar, FALSE, TRUE, 0);

  lives_toolbar_set_style(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOLBAR_ICONS);
  lives_toolbar_set_icon_size(LIVES_TOOLBAR(mainw->toolbar), LIVES_ICON_SIZE_SMALL_TOOLBAR);
  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_MEDIA_STOP,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_stopbutton = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_stopbutton), 0);
  lives_widget_set_tooltip_text(mainw->t_stopbutton, _("Stop playback (q)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_SEPWIN,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_sepwin = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_sepwin), 2);
  lives_widget_set_tooltip_text(mainw->t_sepwin, _("Play in separate window (s)"));
  lives_widget_set_opacity(mainw->t_sepwin, .75);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_LIVES_STOCK_FULLSCREEN,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_fullscreen = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_fullscreen), 4);
  lives_widget_set_tooltip_text(mainw->t_fullscreen, _("Fullscreen playback (f)"));
  lives_widget_set_opacity(mainw->t_fullscreen, .75);

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_REMOVE, lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_slower = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_slower), 5);
  lives_widget_set_tooltip_text(mainw->t_slower, _("Play slower (ctrl-down)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_ADD,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_faster = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_faster), 6);
  lives_widget_set_tooltip_text(mainw->t_faster, _("Play faster (ctrl-up)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_GO_BACK,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_back = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_back), 7);
  lives_widget_set_tooltip_text(mainw->t_back, _("Skip back (ctrl-left)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_GO_FORWARD,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_forward = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_forward), 8);
  lives_widget_set_tooltip_text(mainw->t_forward, _("Skip forward (ctrl-right)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_DIALOG_INFO,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_infobutton = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_infobutton), 9);
  lives_widget_set_tooltip_text(mainw->t_infobutton, _("Show clip info (i)"));

  tmp_toolbar_icon = lives_image_new_from_stock(LIVES_STOCK_CLOSE,
                     lives_toolbar_get_icon_size(LIVES_TOOLBAR(mainw->toolbar)));

  mainw->t_hide = LIVES_WIDGET(lives_tool_button_new(LIVES_WIDGET(tmp_toolbar_icon), ""));
  lives_toolbar_insert(LIVES_TOOLBAR(mainw->toolbar), LIVES_TOOL_ITEM(mainw->t_hide), 10);
  lives_widget_set_tooltip_text(mainw->t_hide, _("Hide this toolbar"));

  t_label = lives_label_new(_("Press \"s\" to toggle separate play window for improved performance, \"q\" to stop."));

  if (palette->style & STYLE_1) {
    lives_widget_set_fg_color(t_label, LIVES_WIDGET_STATE_NORMAL, &palette->banner_fade_text);
  }

  lives_box_pack_start(LIVES_BOX(mainw->tb_hbox), t_label, FALSE, FALSE, 0);

  // framebar menu bar

  vbox99 = lives_vbox_new(FALSE, 0);

  mainw->eventbox = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->eventbox, FALSE, FALSE, 0);
  lives_widget_add_events(mainw->eventbox, LIVES_SMOOTH_SCROLL_MASK | LIVES_SCROLL_MASK);

  lives_container_add(LIVES_CONTAINER(mainw->eventbox), vbox99);

  vbox4 = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox99), vbox4, FALSE, TRUE, 0);
  lives_widget_set_vexpand(vbox4, FALSE);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox), LIVES_WIDGET_SCROLL_EVENT,
                            LIVES_GUI_CALLBACK(on_mouse_scroll), NULL);

  mainw->framebar = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->framebar, FALSE, FALSE, 0.);

  mainw->vps_label = lives_standard_label_new(_("Video playback speed (frames per second)"));
  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->vps_label, FALSE, FALSE, widget_opts.packing_width);

  widget_opts.expand = LIVES_EXPAND_NONE;
  mainw->spinbutton_pb_fps = lives_standard_spin_button_new(NULL, 0., -FPS_MAX, FPS_MAX, 0.0001, 1., 3,
                             LIVES_BOX(mainw->framebar), _("Vary the video speed"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_widget_set_sensitive(mainw->spinbutton_pb_fps, FALSE);

  if (palette->style == STYLE_PLAIN) {
    mainw->banner = lives_label_new("             = <  L i V E S > =                ");
  } else {
    mainw->banner = lives_label_new("                                                ");
  }
  lives_widget_set_halign(mainw->banner, LIVES_ALIGN_CENTER);

  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->banner, TRUE, TRUE, 0);

  mainw->framecounter = lives_standard_entry_new("", "", FCWIDTHCHARS, FCWIDTHCHARS, NULL, NULL);

#if GTK_CHECK_VERSION(3, 16, 0)
  if (mainw->pretty_colours) {
    set_css_value_direct(LIVES_WIDGET(mainw->framecounter), LIVES_WIDGET_STATE_NORMAL, "", "border-top-left-radius", "16px");
    set_css_value_direct(LIVES_WIDGET(mainw->framecounter), LIVES_WIDGET_STATE_NORMAL, "", "border-top-right-radius", "16px");
    set_css_value_direct(LIVES_WIDGET(mainw->framecounter), LIVES_WIDGET_STATE_NORMAL, "", "border-bottom-right-radius", "16px");
    set_css_value_direct(LIVES_WIDGET(mainw->framecounter), LIVES_WIDGET_STATE_NORMAL, "", "border-bottom-left-radius", "16px");
  }
#endif

  lives_box_pack_start(LIVES_BOX(mainw->framebar), mainw->framecounter, FALSE, TRUE, 0);
  lives_entry_set_editable(LIVES_ENTRY(mainw->framecounter), FALSE);
  lives_widget_set_sensitive(mainw->framecounter, TRUE);
  lives_widget_set_can_focus(mainw->framecounter, FALSE);

  add_fill_to_box(LIVES_BOX(mainw->framebar));

  mainw->pf_grid = lives_table_new(1, 3, TRUE);
  lives_widget_set_vexpand(mainw->pf_grid, FALSE);
  lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), TRUE);

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->pf_grid, FALSE, FALSE, 0);
#else
  // for gtk+ 2.x
  hbox = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox4), hbox, FALSE, FALSE, 0);
  add_spring_to_box(LIVES_BOX(hbox), 0);
  alignment = lives_alignment_new(0.5, 0.5, 1.0, 1.0);
  lives_container_add(LIVES_CONTAINER(alignment), mainw->pf_grid);
  add_spring_to_box(LIVES_BOX(hbox), 0);
  lives_box_pack_start(LIVES_BOX(hbox), alignment, TRUE, TRUE, 0);
  lives_widget_set_halign(alignment, LIVES_ALIGN_CENTER);
#endif
#endif

  mainw->eventbox3 = lives_event_box_new();
  lives_table_attach(LIVES_TABLE(mainw->pf_grid), mainw->eventbox3, 0, 1, 0, 1,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);
  lives_widget_set_halign(mainw->eventbox3, LIVES_ALIGN_CENTER);

  lives_widget_set_margin_left(mainw->eventbox3, widget_opts.packing_width);
  lives_widget_set_margin_right(mainw->eventbox3, widget_opts.packing_width);

  widget_opts.expand = LIVES_EXPAND_NONE;
  mainw->frame1 = lives_standard_frame_new(_("First Frame"), 0.25, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  lives_container_add(LIVES_CONTAINER(mainw->eventbox3), mainw->frame1);

  mainw->freventbox0 = lives_event_box_new();
  lives_container_add(LIVES_CONTAINER(mainw->frame1), mainw->freventbox0);
  lives_container_add(LIVES_CONTAINER(mainw->freventbox0), mainw->start_image);

  widget_opts.expand = LIVES_EXPAND_NONE;
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  mainw->playframe = lives_standard_frame_new(_("Play"), 0.5, TRUE);
  lives_widget_set_app_paintable(mainw->playframe, TRUE);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  mainw->pl_eventbox = lives_event_box_new();

  lives_container_set_border_width(LIVES_CONTAINER(mainw->playframe), 0);

  lives_widget_set_hexpand(mainw->playframe, TRUE);

  lives_widget_set_margin_left(mainw->playframe, widget_opts.packing_width);
  lives_widget_set_margin_right(mainw->playframe, widget_opts.packing_width);

  lives_container_add(LIVES_CONTAINER(mainw->playframe), mainw->pl_eventbox);
  //lives_widget_set_size_request(mainw->playframe, -1, GUI_SCREEN_HEIGHT / 4);
  lives_widget_set_hexpand(mainw->pl_eventbox, FALSE);

  mainw->playarea = lives_event_box_new();
  lives_container_add(LIVES_CONTAINER(mainw->pl_eventbox), mainw->playarea);
  lives_widget_set_app_paintable(mainw->playarea, TRUE);

  lives_table_attach(LIVES_TABLE(mainw->pf_grid), mainw->playframe, 1, 2, 0, 1,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_widget_set_halign(mainw->playframe, LIVES_ALIGN_CENTER);

  mainw->eventbox4 = lives_event_box_new();

  lives_widget_set_app_paintable(mainw->eventbox4, TRUE);

  lives_table_attach(LIVES_TABLE(mainw->pf_grid), mainw->eventbox4, 2, 3, 0, 1,
                     (LiVESAttachOptions)(0), (LiVESAttachOptions)(0), 0, 0);

  lives_widget_set_halign(mainw->eventbox4, LIVES_ALIGN_CENTER);

  lives_widget_set_margin_left(mainw->eventbox4, widget_opts.packing_width);
  lives_widget_set_margin_right(mainw->eventbox4, widget_opts.packing_width);

  widget_opts.expand = LIVES_EXPAND_NONE;
  mainw->frame2 = lives_standard_frame_new(_("Last Frame"), 0.75, TRUE);
  widget_opts.expand = LIVES_EXPAND_DEFAULT;

  lives_container_add(LIVES_CONTAINER(mainw->eventbox4), mainw->frame2);

  mainw->freventbox1 = lives_event_box_new();
  lives_container_add(LIVES_CONTAINER(mainw->frame2), mainw->freventbox1);
  lives_container_add(LIVES_CONTAINER(mainw->freventbox1), mainw->end_image);

  if (mainw->imframe) {
    if (lives_pixbuf_get_width(mainw->imframe) + H_RESIZE_ADJUST < mainw->def_width) {
      mainw->def_width = lives_pixbuf_get_width(mainw->imframe) + H_RESIZE_ADJUST;
    }
    if (lives_pixbuf_get_height(mainw->imframe) + V_RESIZE_ADJUST * mainw->foreign < mainw->def_height) {
      mainw->def_height = lives_pixbuf_get_height(mainw->imframe) + V_RESIZE_ADJUST * mainw->foreign;
    }
  }

  // the actual playback image for the internal player
  mainw->play_image = lives_standard_drawing_area_new(NULL, &mainw->play_surface);
  lives_widget_show(mainw->play_image); // needed to get size
  lives_widget_apply_theme(mainw->play_image, LIVES_WIDGET_STATE_NORMAL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->play_image), LIVES_WIDGET_EXPOSE_EVENT,
                            LIVES_GUI_CALLBACK(all_expose), (livespointer)&mainw->play_surface);

  lives_widget_object_ref(mainw->play_image);
  lives_widget_object_ref_sink(mainw->play_image);

  mainw->hbox3 = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(vbox4), mainw->hbox3, FALSE, FALSE, 0);
  add_spring_to_box(LIVES_BOX(mainw->hbox3), 0);

  // "start" spin

  dpw = widget_opts.packing_width;
  widget_opts.expand = LIVES_EXPAND_NONE;
#if GTK_CHECK_VERSION(3, 0, 0)
  if (!(palette->style & STYLE_2)) {
    widget_opts.apply_theme = 0;
  }
#else
  widget_opts.apply_theme = 0;
#endif
  widget_opts.packing_width = MAIN_SPIN_SPACER;
  mainw->spinbutton_start = lives_standard_spin_button_new(NULL, 0., 0., 0., 1., 1., 0,
                            LIVES_BOX(mainw->hbox3), _("The first selected frame in this clip"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.packing_width = dpw;
  widget_opts.apply_theme = woat;

  if (woat) {
    set_css_value_direct(mainw->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
    set_css_value_direct(mainw->spinbutton_start, LIVES_WIDGET_STATE_INSENSITIVE, "button", "opacity", "0.5");
  }
  lives_entry_set_width_chars(LIVES_ENTRY(mainw->spinbutton_start), 10);
  lives_widget_set_halign(mainw->spinbutton_start, LIVES_ALIGN_CENTER);
  add_spring_to_box(LIVES_BOX(mainw->hbox3), 0);

  // arrows and label

  vbox = lives_vbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->hbox3), vbox, FALSE, FALSE, 0);
  lives_widget_set_halign(vbox, LIVES_ALIGN_CENTER);

  //

  hbox = lives_hbox_new(FALSE, 0.);
  lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, 0);
  lives_widget_set_valign(hbox, LIVES_ALIGN_START);
  lives_widget_set_halign(hbox, LIVES_ALIGN_CENTER);

  mainw->arrow1 = lives_arrow_new(LIVES_ARROW_LEFT, LIVES_SHADOW_OUT);
  lives_box_pack_start(LIVES_BOX(hbox), mainw->arrow1, FALSE, FALSE, 0);

  mainw->sel_label = lives_standard_label_new(NULL);

  set_sel_label(mainw->sel_label);

  lives_box_pack_start(LIVES_BOX(hbox), mainw->sel_label, FALSE, FALSE, 0);

  mainw->arrow2 = lives_arrow_new(LIVES_ARROW_RIGHT, LIVES_SHADOW_OUT);
  lives_box_pack_start(LIVES_BOX(hbox), mainw->arrow2, FALSE, FALSE, 0);

  mainw->sa_hbox = lives_hbox_new(TRUE, 0);
  add_fill_to_box(LIVES_BOX(mainw->sa_hbox));
  lives_box_pack_start(LIVES_BOX(vbox), mainw->sa_hbox, FALSE, FALSE,
                       lives_widget_set_margin_top(mainw->sa_hbox, 4) ? 0 : 4);

  mainw->sa_button = lives_standard_button_new_full(_("Select all Frames"), DEF_BUTTON_WIDTH,
                     DEF_BUTTON_HEIGHT, LIVES_BOX(mainw->sa_hbox), TRUE,
                     (tmp = (_("Select all frames in this clip"))));
  lives_free(tmp);
  add_fill_to_box(LIVES_BOX(mainw->sa_hbox));
  lives_widget_set_halign(mainw->sa_button, LIVES_ALIGN_CENTER);

  add_spring_to_box(LIVES_BOX(mainw->hbox3), 0);

  widget_opts.expand = LIVES_EXPAND_NONE;
  widget_opts.packing_width = MAIN_SPIN_SPACER;
#if GTK_CHECK_VERSION(3, 0, 0)
  if (!(palette->style & STYLE_2)) {
    widget_opts.apply_theme = 0;
  }
#else
  widget_opts.apply_theme = 0;
#endif
  mainw->spinbutton_end = lives_standard_spin_button_new(NULL, 0., 0., 0., 1., 1., 0,
                          LIVES_BOX(mainw->hbox3), _("The last selected frame in this clip"));
  widget_opts.expand = LIVES_EXPAND_DEFAULT;
  widget_opts.packing_width = dpw;
  widget_opts.apply_theme = woat;
  if (woat) {
    set_css_value_direct(mainw->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, "", "opacity", "0.5");
    set_css_value_direct(mainw->spinbutton_end, LIVES_WIDGET_STATE_INSENSITIVE, "button", "opacity", "0.5");
  }

  add_spring_to_box(LIVES_BOX(mainw->hbox3), 0.);
  lives_entry_set_width_chars(LIVES_ENTRY(mainw->spinbutton_end), 10);
  lives_widget_set_halign(mainw->spinbutton_end, LIVES_ALIGN_CENTER);

  lives_widget_set_sensitive(mainw->spinbutton_start, FALSE);
  lives_widget_set_sensitive(mainw->spinbutton_end, FALSE);

  mainw->hseparator = lives_hseparator_new();

  if (palette->style & STYLE_1) {
    lives_box_pack_start(LIVES_BOX(vbox4), mainw->sep_image, FALSE, FALSE, 4. * widget_opts.scale);
  } else {
    lives_box_pack_start(LIVES_BOX(vbox4), mainw->hseparator, TRUE, TRUE, 0);
  }

#ifdef ENABLE_GIW_3
  mainw->hruler = giw_timeline_new_with_adjustment(LIVES_ORIENTATION_HORIZONTAL, 0., 0., 1000000., 1000000.);
  lives_box_pack_start(LIVES_BOX(vbox99), mainw->hruler, FALSE, FALSE, 0);
  mainw->eventbox5 = NULL;
#else
  mainw->eventbox5 = lives_event_box_new();
  lives_box_pack_start(LIVES_BOX(vbox99), mainw->eventbox5, FALSE, FALSE, 0);
  mainw->hruler = lives_standard_hruler_new();
  lives_ruler_set_range(LIVES_RULER(mainw->hruler), 0., 1000000., 0., 1000000.);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox5), mainw->hruler);
  lives_widget_add_events(mainw->eventbox5, LIVES_BUTTON1_MOTION_MASK | LIVES_POINTER_MOTION_MASK
                          | LIVES_BUTTON_RELEASE_MASK | LIVES_BUTTON_PRESS_MASK | LIVES_ENTER_NOTIFY_MASK);
#endif

  lives_widget_set_size_request(mainw->hruler, -1, CE_HRULE_HEIGHT);

  mainw->eventbox2 = lives_event_box_new();

  vbox2 = lives_vbox_new(FALSE, 0);
  lives_container_add(LIVES_CONTAINER(mainw->eventbox2), vbox2);
  lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->eventbox2, FALSE, TRUE, 0);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_EXPOSE_EVENT,
                                  LIVES_GUI_CALLBACK(all_expose_overlay), NULL);

  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  mainw->vidbar = lives_standard_label_new(_("Video"));
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->vidbar, FALSE, FALSE,
                       lives_widget_set_margin(mainw->vidbar,
                           widget_opts.packing_height * 2) ? 0 : widget_opts.packing_height * 2);

  widget_opts.apply_theme = 1;
  mainw->video_draw = lives_drawing_area_new();
  widget_opts.apply_theme = woat;

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->video_draw), LIVES_WIDGET_EXPOSE_EVENT,
                            LIVES_GUI_CALLBACK(expose_vid_draw), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->video_draw), LIVES_WIDGET_CONFIGURE_EVENT,
                                  LIVES_GUI_CALLBACK(config_vid_draw), NULL);

  lives_widget_set_size_request(mainw->video_draw, -1, CE_VIDBAR_HEIGHT);
  lives_widget_set_hexpand(mainw->video_draw, TRUE);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->video_draw, FALSE, TRUE, widget_opts.packing_height / 2);

  tmp = get_achannel_name(2, 0);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  mainw->laudbar = lives_standard_label_new(tmp);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_free(tmp);

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->laudbar, FALSE, FALSE,
                       lives_widget_set_margin_top(mainw->laudbar,
                           widget_opts.packing_height + 4) ? 0 : widget_opts.packing_height + 4);

  widget_opts.apply_theme = 1;
  mainw->laudio_draw = lives_drawing_area_new();
  widget_opts.apply_theme = woat;

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->laudio_draw), LIVES_WIDGET_EXPOSE_EVENT,
                            LIVES_GUI_CALLBACK(expose_laud_draw), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->laudio_draw), LIVES_WIDGET_CONFIGURE_EVENT,
                                  LIVES_GUI_CALLBACK(config_laud_draw), NULL);

  lives_widget_set_size_request(mainw->laudio_draw, -1, CE_AUDBAR_HEIGHT);
  lives_widget_set_hexpand(mainw->laudio_draw, TRUE);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->laudio_draw, FALSE, TRUE, widget_opts.packing_height / 2);

  tmp = get_achannel_name(2, 1);
  widget_opts.justify = LIVES_JUSTIFY_CENTER;
  mainw->raudbar = lives_standard_label_new(tmp);
  widget_opts.justify = LIVES_JUSTIFY_DEFAULT;
  lives_free(tmp);

  lives_box_pack_start(LIVES_BOX(vbox2), mainw->raudbar, FALSE, FALSE, 0);
  /* lives_widget_set_margin_top(mainw->raudbar, */
  /* 				   widget_opts.packing_height) ? 0 : widget_opts.packing_height); */
  widget_opts.apply_theme = 1;
  mainw->raudio_draw = lives_drawing_area_new();
  widget_opts.apply_theme = woat;

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->raudio_draw), LIVES_WIDGET_EXPOSE_EVENT,
                            LIVES_GUI_CALLBACK(expose_raud_draw), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->raudio_draw), LIVES_WIDGET_CONFIGURE_EVENT,
                                  LIVES_GUI_CALLBACK(config_raud_draw), NULL);

  lives_widget_set_size_request(mainw->raudio_draw, -1, CE_AUDBAR_HEIGHT);
  lives_widget_set_hexpand(mainw->raudio_draw, TRUE);
  lives_box_pack_start(LIVES_BOX(vbox2), mainw->raudio_draw, FALSE, FALSE, 0);

  if (prefs->show_msg_area) {
    mainw->message_box = lives_hbox_new(FALSE, 0);
    lives_widget_set_app_paintable(mainw->message_box, TRUE);
    //lives_widget_set_vexpand(mainw->message_box, TRUE);
    lives_box_pack_start(LIVES_BOX(mainw->top_vbox), mainw->message_box, FALSE, TRUE, 0);

    mainw->msg_area = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(reshow_msg_area), &mainw->msg_surface);

    lives_widget_add_events(mainw->msg_area, LIVES_SMOOTH_SCROLL_MASK | LIVES_SCROLL_MASK);

    //lives_widget_set_vexpand(mainw->msg_area, TRUE);
    lives_container_set_border_width(LIVES_CONTAINER(mainw->message_box), 0);
    lives_widget_apply_theme3(mainw->msg_area, LIVES_WIDGET_STATE_NORMAL);
    lives_box_pack_start(LIVES_BOX(mainw->message_box), mainw->msg_area, TRUE, TRUE, 0);

    mainw->msg_scrollbar = lives_vscrollbar_new(NULL);
    lives_box_pack_end(LIVES_BOX(mainw->message_box), mainw->msg_scrollbar, FALSE, TRUE, 0);

    mainw->msg_adj = lives_range_get_adjustment(LIVES_RANGE(mainw->msg_scrollbar));

    lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->msg_adj), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(msg_area_scroll), (livespointer)mainw->msg_area);

    lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->msg_area), LIVES_WIDGET_SCROLL_EVENT,
                              LIVES_GUI_CALLBACK(on_msg_area_scroll), (livespointer)mainw->msg_adj);
  } else {
    mainw->message_box = mainw->msg_area = mainw->msg_scrollbar = NULL;
    mainw->msg_adj = NULL;
  }

  // accel keys
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Page_Up, LIVES_CONTROL_MASK,
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(prevclip_callback), NULL, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_KP_Page_Up, LIVES_CONTROL_MASK,
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(prevclip_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Page_Down, LIVES_CONTROL_MASK,
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(nextclip_callback), NULL, NULL));
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_KP_Page_Down, LIVES_CONTROL_MASK,
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(nextclip_callback), NULL, NULL));

  // Let the player handle this

  // actually, for up and down, the user data doesn't matter since it is ignored when we create synthetic
  // key presses, and only the keymod counts. However it's added here as a guide.
  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Down,
                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(slower_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Down,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(slower_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Down,
                            (LIVES_ALT_MASK), (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(less_callback), NULL, NULL));

  //////////////////////////

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Up,
                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(faster_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Up,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(faster_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Up,
                            (LiVESXModifierType)(LIVES_ALT_MASK),
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(more_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Left,
                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(skip_back_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Right,
                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(skip_forward_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Space,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK),
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(dirchange_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Space,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_ALT_MASK),
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(dirchange_lock_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), GDK_KEY_braceleft,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK),
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(dirchange_lock_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Space,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_SHIFT_MASK),
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(dirchange_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Return,
                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(fps_reset_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Return,
                            (LiVESXModifierType)(LIVES_CONTROL_MASK | LIVES_SHIFT_MASK), (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(fps_reset_callback),
                                LIVES_INT_TO_POINTER(SCREEN_AREA_BACKGROUND), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Less, (LiVESXModifierType)LIVES_SHIFT_MASK,
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(voldown_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Greater, (LiVESXModifierType)LIVES_SHIFT_MASK,
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(volup_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_k, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(-1), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_t, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(textparm_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_m, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(rtemode_callback), NULL,
                                LIVES_INT_TO_POINTER(NEXT_MODE_CYCLE)));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_m, (LiVESXModifierType)LIVES_SHIFT_MASK,
                            (LiVESAccelFlags)0, lives_cclosure_new(LIVES_GUI_CALLBACK(rtemode_callback),
                                LIVES_INT_TO_POINTER(PREV_MODE_CYCLE), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_x, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(swap_fg_bg_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_n, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(nervous_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_w, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(show_sync_callback), NULL, NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_a, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(aud_lock_callback), LIVES_INT_TO_POINTER(TRUE), NULL));

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_a, (LiVESXModifierType)LIVES_SHIFT_MASK,
                            (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(aud_lock_callback), LIVES_INT_TO_POINTER(FALSE), NULL));

  if (FN_KEYS > 0) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F1, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(1), NULL));
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F1, (LiVESXModifierType)LIVES_SHIFT_MASK,
                              (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(1), NULL));
    if (FN_KEYS > 1) {
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F2, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(2), NULL));
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F2, (LiVESXModifierType)LIVES_SHIFT_MASK,
                                (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(2), NULL));
      if (FN_KEYS > 2) {
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F3, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(3), NULL));
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F3, (LiVESXModifierType)LIVES_SHIFT_MASK,
                                  (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(3), NULL));
        if (FN_KEYS > 3) {
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F4,
                                    (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(4), NULL));
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F4,
                                    (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(4), NULL));
          if (FN_KEYS > 4) {
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F5,
                                      (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(5), NULL));
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F5,
                                      (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback), LIVES_INT_TO_POINTER(5), NULL));
            if (FN_KEYS > 5) {
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F6,
                                        (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                            LIVES_INT_TO_POINTER(6), NULL));
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F6,
                                        (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                            LIVES_INT_TO_POINTER(6), NULL));
              if (FN_KEYS > 6) {
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F7,
                                          (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                              LIVES_INT_TO_POINTER(7), NULL));
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F7,
                                          (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                              LIVES_INT_TO_POINTER(7), NULL));
                if (FN_KEYS > 7) {
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F8,
                                            (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                LIVES_INT_TO_POINTER(8), NULL));
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F8,
                                            (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                LIVES_INT_TO_POINTER(8), NULL));
                  if (FN_KEYS > 8) {
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F9,
                                              (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                  LIVES_INT_TO_POINTER(9), NULL));
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F9,
                                              (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                  LIVES_INT_TO_POINTER(9), NULL));
                    if (FN_KEYS > 9) {
                      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F10,
                                                (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                    LIVES_INT_TO_POINTER(10), NULL));
                      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F10,
                                                (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                                lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                    LIVES_INT_TO_POINTER(10), NULL));
                      if (FN_KEYS > 10) {
                        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F11,
                                                  (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                      LIVES_INT_TO_POINTER(11), NULL));
                        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F11,
                                                  (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                                  lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                      LIVES_INT_TO_POINTER(11), NULL));
                        if (FN_KEYS > 11) {
                          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F12,
                                                    (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                        LIVES_INT_TO_POINTER(12), NULL));
                          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_F12,
                                                    (LiVESXModifierType)LIVES_SHIFT_MASK, (LiVESAccelFlags)0,
                                                    lives_cclosure_new(LIVES_GUI_CALLBACK(storeclip_callback),
                                                        LIVES_INT_TO_POINTER(12), NULL));
                          // ad nauseum...
			  // *INDENT-OFF*
                        }}}}}}}}}}}}
  // *INDENT-ON*

  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_0, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                            lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(0), NULL));
  if (FX_KEYS_PHYSICAL > 0) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_1, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(1), NULL));
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_1, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                              (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(0), NULL));
    if (FX_KEYS_PHYSICAL > 1) {
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_2, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(2), NULL));
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_2, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                (LiVESAccelFlags)0,
                                lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(1), NULL));
      if (FX_KEYS_PHYSICAL > 2) {
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_3, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(3), NULL));
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_3, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                  (LiVESAccelFlags)0,
                                  lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(2), NULL));
        if (FX_KEYS_PHYSICAL > 3) {
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_4, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(4), NULL));
          lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_4, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                    (LiVESAccelFlags)0,
                                    lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(3), NULL));
          if (FX_KEYS_PHYSICAL > 4) {
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_5, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(5), NULL));
            lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_5, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                      (LiVESAccelFlags)0,
                                      lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(4), NULL));
            if (FX_KEYS_PHYSICAL > 5) {
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_6,
                                        LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),
                                            LIVES_INT_TO_POINTER(6), NULL));
              lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_6, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                        (LiVESAccelFlags)0,
                                        lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback), LIVES_INT_TO_POINTER(5), NULL));
              if (FX_KEYS_PHYSICAL > 6) {
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_7,
                                          LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),
                                              LIVES_INT_TO_POINTER(7), NULL));
                lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_7, LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                          (LiVESAccelFlags)0,
                                          lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback),
                                              LIVES_INT_TO_POINTER(6), NULL));
                if (FX_KEYS_PHYSICAL > 7) {
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_8,
                                            LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),
                                                LIVES_INT_TO_POINTER(8), NULL));
                  lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_8,
                                            LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                            (LiVESAccelFlags)0,
                                            lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback),
                                                LIVES_INT_TO_POINTER(7), NULL));
                  if (FX_KEYS_PHYSICAL > 8) {
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group),
                                              LIVES_KEY_9, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback),
                                                  LIVES_INT_TO_POINTER(9), NULL));
                    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_9,
                                              LIVES_CONTROL_MASK | LIVES_ALT_MASK,
                                              (LiVESAccelFlags)0,
                                              lives_cclosure_new(LIVES_GUI_CALLBACK(grabkeys_callback),
                                                  LIVES_INT_TO_POINTER(8), NULL));
		    // *INDENT-OFF*
                  }}}}}}}}}
  // *INDENT-ON*

  if (prefs->rte_keys_virtual > 9) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Minus, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(10), NULL));
  }
  if (prefs->rte_keys_virtual > 10) {
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_Equal, LIVES_CONTROL_MASK, (LiVESAccelFlags)0,
                              lives_cclosure_new(LIVES_GUI_CALLBACK(rte_on_off_callback), LIVES_INT_TO_POINTER(11), NULL));
  }

  if (new_lives) {
    lives_signal_connect(LIVES_GUI_OBJECT(LIVES_MAIN_WINDOW_WIDGET), LIVES_WIDGET_DELETE_EVENT,
                         LIVES_GUI_CALLBACK(on_LiVES_delete_event), NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(LIVES_MAIN_WINDOW_WIDGET), LIVES_WIDGET_KEY_PRESS_EVENT,
                         LIVES_GUI_CALLBACK(key_press_or_release), NULL);
    lives_signal_connect(LIVES_GUI_OBJECT(LIVES_MAIN_WINDOW_WIDGET), LIVES_WIDGET_KEY_RELEASE_EVENT,
                         LIVES_GUI_CALLBACK(key_press_or_release), NULL);
  }
  mainw->pb_fps_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_pb_fps),
                       LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                       LIVES_GUI_CALLBACK(changed_fps_during_pb), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->open), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_open_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_sel_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_dvd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(LIVES_DEVICE_DVD));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_vcd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_vcd_activate),
                       LIVES_INT_TO_POINTER(LIVES_DEVICE_VCD));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_loc), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_loc_activate), NULL);
  //#ifdef HAVE_WEBM
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->open_utube), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_open_utube_activate), NULL);

#ifdef HAVE_LDVGRAB
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_firewire), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_fw_activate),
                       LIVES_INT_TO_POINTER(CAM_FORMAT_DV));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_hfirewire), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_fw_activate),
                       LIVES_INT_TO_POINTER(CAM_FORMAT_HDV));
#endif

#ifdef HAVE_YUV4MPEG
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_yuv4m), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_yuv4m_activate), NULL);
#endif
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->open_lives2lives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_open_lives2lives_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->send_lives2lives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_send_lives2lives_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->backup), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_backup_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->restore), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_restore_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->save_as), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_as_activate), NULL);
#ifdef LIBAV_TRANSCODE
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->transcode), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_transcode_activate), NULL);
#endif
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->save_selection), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_save_selection_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->close), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_close_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->import_proj), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_import_proj_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->export_proj), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_export_proj_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->export_theme), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_export_theme_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->import_theme), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_import_theme_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->clear_ds), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cleardisk_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->quit), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_quit_activate),
                            LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_save_set), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_quit_activate),
                       LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->undo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_undo_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->redo), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_redo_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->copy), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_copy_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->mt_menu), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_multitrack_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->cut), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_cut_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->insert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_insert_pre_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->merge), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_merge_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->paste_as_new), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_paste_as_new_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->xdelete), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_delete_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_all), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_all_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_start_only), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_start_only_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_end_only), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_end_only_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_invert), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_invert_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_new), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_new_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_to_end), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_to_end_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_to_aend), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_to_aend_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_from_start), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_from_start_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->select_last), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_select_last_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->lock_selwidth), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_lock_selwidth_activate), NULL);
  mainw->record_perf_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->record_perf), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_record_perf_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playall), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rewind), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rewind_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playsel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playsel_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->playclip), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playclip_activate), NULL);
  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->stop), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_stop_activate),
                                  NULL);  // connect after to stop keypress propagating to removed fs window
  mainw->fullscreen_cb_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->full_screen), LIVES_WIDGET_ACTIVATE_SIGNAL,
                              LIVES_GUI_CALLBACK(on_full_screen_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->sw_sound), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_boolean_toggled),
                            &mainw->save_with_sound); // TODO - make pref
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->showsubs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_showsubs_toggled), NULL);
  mainw->lb_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->letter), LIVES_WIDGET_ACTIVATE_SIGNAL,
                   LIVES_GUI_CALLBACK(toggle_sets_pref), PREF_LETTERBOX);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->aload_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_boolean_toggled), &prefs->autoload_subs);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->ccpd_sound), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_boolean_toggled),
                            &mainw->ccpd_with_sound); // TODO - make pref
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->dsize), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_double_size_activate), NULL);
  mainw->sepwin_cb_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->sepwin), LIVES_WIDGET_ACTIVATE_SIGNAL,
                          LIVES_GUI_CALLBACK(on_sepwin_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->fade), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_fade_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->loop_video), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_loop_video_activate), NULL);
  mainw->loop_cont_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->loop_continue), LIVES_WIDGET_ACTIVATE_SIGNAL,
                          LIVES_GUI_CALLBACK(on_loop_cont_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->loop_ping_pong), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_ping_pong_activate), NULL);
  mainw->mute_audio_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->mute_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                           LIVES_GUI_CALLBACK(on_mute_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->sticky), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_sticky_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->showfct), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_showfct_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->preferences), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_preferences_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->change_speed), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_change_speed_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->resample_video), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_resample_video_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->load_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_load_subs_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->erase_subs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_erase_subs_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->capture), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_capture_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->rev_clipboard), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rev_clipboard_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->export_selaudio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_export_audio_activate), LIVES_INT_TO_POINTER(0));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->export_allaudio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_export_audio_activate), LIVES_INT_TO_POINTER(1));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->append_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_append_audio_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->normalize_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_normalise_audio_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->trim_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_trim_audio_activate), LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->trim_to_pstart), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_trim_audio_activate), LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delsel_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_del_audio_activate), LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->voladj), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_voladj_activate), LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->fade_aud_in), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fade_audio_activate), LIVES_INT_TO_POINTER(0));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->fade_aud_out), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_fade_audio_activate), LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->delall_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_del_audio_activate), LIVES_INT_TO_POINTER(1));
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->ins_silence), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_ins_silence_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recaudio_clip), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recaudclip_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->recaudio_sel), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_recaudsel_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->resample_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_resample_audio_activate), NULL);
  /* lives_signal_connect(LIVES_GUI_OBJECT(mainw->adj_audio_sync), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_adj_audio_sync_activate), NULL);*/
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->load_audio), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_load_audio_activate), NULL);
  if (check_for_executable(&capable->has_cdda2wav, EXEC_CDDA2WAV)
      || check_for_executable(&capable->has_icedax, EXEC_ICEDAX)) {
    lives_signal_connect(LIVES_GUI_OBJECT(mainw->load_cdtrack), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_load_cdtrack_activate), NULL);

    lives_signal_connect(LIVES_GUI_OBJECT(mainw->eject_cd), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_eject_cd_activate), NULL);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_file_info), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_file_info_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_file_comments), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_file_comments_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_clipboard_info), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_clipboard_info_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_messages), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_messages_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_layout_errors), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(popup_lmap_errors), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->show_quota), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(run_diskspace_dialog_cb), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->rename), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_rename_activate), NULL);

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
  lives_signal_sync_connect(LIVES_GUI_OBJECT(rebuild_rfx), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_rebuild_rfx_activate),
                            LIVES_INT_TO_POINTER(RFX_STATUS_TEST));
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
                       LIVES_GUI_CALLBACK(on_load_set_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->vj_show_keys), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_keys_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(assign_rte_keys), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_assign_rte_keys_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->save_rte_defs), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_save_rte_defs_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->vj_reset), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_vj_reset_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->vj_realize), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vj_realize_activate), NULL);
#ifdef ENABLE_OSC
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->midi_learn), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_midi_learn_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->midi_save), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_devicemap_save_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(midi_load), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_devicemap_load_activate), NULL);
#endif

  mainw->toy_func_none = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->toy_none), LIVES_WIDGET_ACTIVATE_SIGNAL,
                         LIVES_GUI_CALLBACK(on_toy_activate), NULL);

  lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->autolives), LIVES_WIDGET_ACTIVATE_SIGNAL,
                                  LIVES_GUI_CALLBACK(autolives_toggle), NULL);

  mainw->toy_func_random_frames = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->toy_random_frames),
                                  LIVES_WIDGET_ACTIVATE_SIGNAL,
                                  LIVES_GUI_CALLBACK(on_toy_activate),
                                  LIVES_INT_TO_POINTER(LIVES_TOY_MAD_FRAMES));

  mainw->toy_func_lives_tv = lives_signal_connect_after(LIVES_GUI_OBJECT(mainw->toy_tv), LIVES_WIDGET_ACTIVATE_SIGNAL,
                             LIVES_GUI_CALLBACK(on_toy_activate), LIVES_INT_TO_POINTER(LIVES_TOY_TV));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(about), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(on_about_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->troubleshoot), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(on_troubleshoot_activate), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->expl_missing), LIVES_WIDGET_ACTIVATE_SIGNAL,
                            LIVES_GUI_CALLBACK(explain_missing_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(show_manual), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(show_manual_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(email_author), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(email_author_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(donate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(donate_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(report_bug), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(report_bug_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(suggest_feature), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(suggest_feature_activate), NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(help_translate), LIVES_WIDGET_ACTIVATE_SIGNAL,
                       LIVES_GUI_CALLBACK(help_translate_activate), NULL);

  mainw->spin_start_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_start),
                           LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                           LIVES_GUI_CALLBACK(on_spinbutton_start_value_changed), NULL);

  mainw->spin_end_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->spinbutton_end),
                         LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                         LIVES_GUI_CALLBACK(on_spinbutton_end_value_changed), NULL);

  // these are for the menu transport buttons
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->m_sepwinbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_sepwin_pressed), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->m_stopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_stop_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_playselbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playsel_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->m_rewindbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_rewind_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->m_mutebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_mute_button_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->m_loopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_loop_button_activate), NULL);

  // these are 'invisible' buttons for the key accelerators
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_stopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_stop_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_sepwin), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_sepwin_pressed), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_fullscreen), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_full_screen_pressed), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_infobutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_show_file_info_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_hide), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_toolbar_hide), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_slower), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_slower_pressed),
                            LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_faster), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_faster_pressed),
                            LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_back), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_back_pressed), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->t_forward), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_forward_pressed), NULL);

  mainw->mouse_fn1 = lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                     LIVES_GUI_CALLBACK(on_mouse_sel_update), NULL);
  lives_signal_handler_block(mainw->eventbox2, mainw->mouse_fn1);
  mainw->mouse_blocked = TRUE;

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                            LIVES_GUI_CALLBACK(on_mouse_sel_reset), NULL);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox2), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(on_mouse_sel_start), NULL);

#ifdef ENABLE_GIW_3
  adj = giw_timeline_get_adjustment(GIW_TIMELINE(mainw->hruler));
  lives_signal_sync_connect_swapped(LIVES_GUI_OBJECT(adj), LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                                    LIVES_GUI_CALLBACK(on_hrule_value_changed), mainw->hruler);
#else
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_MOTION_NOTIFY_EVENT,
                            LIVES_GUI_CALLBACK(on_hrule_update), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_BUTTON_RELEASE_EVENT,
                            LIVES_GUI_CALLBACK(on_hrule_reset), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox5), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(on_hrule_set), NULL);
#endif

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->sa_button), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_select_all_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox3), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(frame_context), LIVES_INT_TO_POINTER(1));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->eventbox4), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(frame_context), LIVES_INT_TO_POINTER(2));

  lives_window_add_accel_group(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), mainw->accel_group);
  mainw->plug = NULL;

  if (prefs->show_msg_area) {
    lives_widget_set_can_focus(mainw->message_box, TRUE);
    if (new_lives) lives_widget_grab_focus(mainw->message_box); // TODO !prefs->show_msg_area
  }

  if (RFX_LOADED) {
    if (mainw->ldg_menuitem) {
      lives_widget_destroy(mainw->ldg_menuitem);
      mainw->ldg_menuitem = NULL;
    }
    add_rfx_effects2(RFX_STATUS_ANY);
  }
}


void show_lives(void) {
  lives_widget_show_all(mainw->top_vbox);
  gtk_window_set_position(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), GTK_WIN_POS_CENTER_ALWAYS);

  if (!prefs->show_gui && prefs->startup_interface == STARTUP_CE) {
    lives_widget_show_now(LIVES_MAIN_WINDOW_WIDGET); //this calls the config_event()
  } else {
    lives_widget_show_all(LIVES_MAIN_WINDOW_WIDGET);
  }

  if (prefs->show_gui) {
    if (palette->style & STYLE_1) {
      set_colours(&palette->normal_fore, &palette->normal_back, &palette->menu_and_bars_fore, &palette->menu_and_bars, \
                  &palette->info_text, &palette->info_base);
    } else {
      set_colours(&palette->normal_fore, &palette->normal_back, &palette->normal_fore, &palette->normal_back, \
                  &palette->normal_fore, &palette->normal_back);
    }
  }
  lives_widget_hide(mainw->redo);
#ifdef LIVES_TV_CHANNEL1
  lives_widget_show(mainw->toy_tv);
#else
  lives_widget_hide(mainw->toy_tv);
# endif
  lives_widget_hide(mainw->tb_hbox);

  if (prefs->hide_framebar || prefs->hfbwnp) {
    lives_widget_hide(mainw->framebar);
  }

  lives_widget_set_opacity(mainw->playframe, 0.);

  if (prefs->show_recent) {
    lives_widget_show(mainw->recent_menu);
  } else {
    lives_widget_hide(mainw->recent_menu);
  }

  for (int i = 0; i < N_RECENT_FILES; i++) {
    const char *mtext = lives_menu_item_get_text(mainw->recent[i]);
    if (*mtext) lives_widget_show(mainw->recent[i]);
  }

  if (!capable->has_composite || !capable->has_convert) {
    lives_widget_hide(mainw->merge);
  }

  if (!LIVES_IS_RANGE(mainw->hruler)) lives_widget_set_sensitive(mainw->hruler, FALSE);

  if (palette->style & STYLE_1 && mainw->current_file == -1) {
    show_playbar_labels(-1);
  }

  if (!((prefs->audio_player == AUD_PLAYER_JACK && capable->has_jackd) ||
        (prefs->audio_player == AUD_PLAYER_PULSE && capable->has_pulse_audio)))
    lives_widget_hide(mainw->vol_toolitem);

  if (mainw->go_away)
    update_rfx_menus();

  if (prefs->present && prefs->show_gui)
    lives_window_present(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
}


void set_interactive(boolean interactive) {
  LiVESList *list;
  LiVESWidget *parent;

  if (!interactive) {
    mainw->sense_state &= ~LIVES_SENSE_STATE_INTERACTIVE;
    if (!mainw->multitrack) {
      parent = lives_widget_get_parent(mainw->menubar);
      if (parent && LIVES_IS_CONTAINER(parent)) {
        lives_widget_object_ref(mainw->menubar);
        lives_widget_unparent(mainw->menubar);
      }
      lives_widget_hide(mainw->btoolbar);
      lives_widget_set_sensitive(mainw->menubar, FALSE);
      lives_widget_set_sensitive(mainw->btoolbar, FALSE);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, mainw->hruler);
    } else {
      parent = lives_widget_get_parent(mainw->multitrack->menubar);
      if (parent && LIVES_IS_CONTAINER(parent)) {
        lives_widget_object_ref(mainw->multitrack->menubar);
        lives_widget_unparent(mainw->multitrack->menubar);
      }
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, mainw->multitrack->timeline);
      lives_widget_set_sensitive(mainw->multitrack->menubar, FALSE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_start, FALSE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_end, FALSE);
      lives_widget_set_sensitive(mainw->m_playbutton, FALSE);
      lives_widget_set_sensitive(mainw->m_stopbutton, FALSE);
      lives_widget_set_sensitive(mainw->m_loopbutton, FALSE);
      lives_widget_set_sensitive(mainw->m_rewindbutton, FALSE);
      lives_widget_set_sensitive(mainw->m_sepwinbutton, FALSE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar2, FALSE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar3, FALSE);
      lives_widget_set_sensitive(mainw->multitrack->snapo_checkbutton, FALSE);
      list = mainw->multitrack->cb_list;
      while (list) {
        lives_widget_set_sensitive((LiVESWidget *)list->data, FALSE);
        list = list->next;
      }
    }
    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->spinbutton_start, FALSE);
    lives_widget_set_sensitive(mainw->spinbutton_end, FALSE);
    lives_widget_set_sensitive(mainw->sa_button, FALSE);

    if (CURRENT_CLIP_IS_VALID && mainw->proc_ptr) {
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, FALSE);
      if (mainw->proc_ptr->stop_button)
        lives_widget_set_sensitive(mainw->proc_ptr->stop_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, FALSE);
      lives_widget_set_sensitive(mainw->proc_ptr->preview_button, FALSE);
    }
  } else {
    mainw->sense_state |= LIVES_SENSE_STATE_INTERACTIVE;
    lives_widget_show(mainw->btoolbar);
    lives_widget_set_sensitive(mainw->menubar, TRUE);
    lives_widget_set_sensitive(mainw->btoolbar, TRUE);

    lives_set_cursor_style(LIVES_CURSOR_CENTER_PTR, mainw->hruler);

    if (mainw->multitrack) {
      lives_set_cursor_style(LIVES_CURSOR_CENTER_PTR, mainw->multitrack->timeline);
      if (!lives_widget_get_parent(mainw->multitrack->menubar)) {
        lives_box_pack_start(LIVES_BOX(mainw->multitrack->menu_hbox), mainw->multitrack->menubar, FALSE, FALSE, 0);
        lives_widget_object_unref(mainw->multitrack->menubar);
      }
      lives_widget_show_all(mainw->multitrack->menubar);

      if (!prefs->show_recent) {
        lives_widget_hide(mainw->multitrack->recent_menu);
      }
      if (!mainw->has_custom_gens) {
        if (mainw->custom_gens_menu) lives_widget_hide(mainw->custom_gens_menu);
        if (mainw->custom_gens_submenu) lives_widget_hide(mainw->custom_gens_submenu);
      }

      lives_widget_hide(mainw->multitrack->aparam_separator); // no longer used

      if (CURRENT_CLIP_HAS_AUDIO) add_aparam_menuitems(mainw->multitrack);
      else {
        lives_widget_hide(mainw->multitrack->render_sep);
        lives_widget_hide(mainw->multitrack->render_vid);
        lives_widget_hide(mainw->multitrack->render_aud);
        lives_widget_hide(mainw->multitrack->normalise_aud);
        lives_widget_hide(mainw->multitrack->view_audio);
        lives_widget_hide(mainw->multitrack->aparam_menuitem);
        lives_widget_hide(mainw->multitrack->aparam_separator);
      }

      if (mainw->multitrack->opts.back_audio_tracks == 0) lives_widget_hide(mainw->multitrack->view_audio);

      lives_widget_set_sensitive(mainw->multitrack->menubar, TRUE);

      lives_widget_set_sensitive(mainw->multitrack->spinbutton_start, TRUE);
      lives_widget_set_sensitive(mainw->multitrack->spinbutton_end, TRUE);

      lives_widget_set_sensitive(mainw->m_rewindbutton, mainw->multitrack->ptr_time > 0.);
      lives_widget_set_sensitive(mainw->m_stopbutton, mainw->multitrack->is_paused);

      lives_widget_set_sensitive(mainw->m_playbutton, TRUE);
      lives_widget_set_sensitive(mainw->m_loopbutton, TRUE);
      lives_widget_set_sensitive(mainw->m_sepwinbutton, TRUE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar2, TRUE);
      lives_widget_set_sensitive(mainw->multitrack->btoolbar3, TRUE);
      lives_widget_set_sensitive(mainw->multitrack->insa_checkbutton, TRUE);
      lives_widget_set_sensitive(mainw->multitrack->snapo_checkbutton, TRUE);
      list = mainw->multitrack->cb_list;
      while (list) {
        lives_widget_set_sensitive((LiVESWidget *)list->data, TRUE);
        list = list->next;
      }
    }

    if (is_realtime_aplayer(prefs->audio_player) && prefs->audio_player != AUD_PLAYER_NONE) {
      lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
      lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
    }
    lives_widget_set_sensitive(mainw->spinbutton_start, TRUE);
    lives_widget_set_sensitive(mainw->spinbutton_end, TRUE);
    lives_widget_set_sensitive(mainw->sa_button, CURRENT_CLIP_HAS_VIDEO && (cfile->start > 1 || cfile->end < cfile->frames));

    if (CURRENT_CLIP_IS_VALID && mainw->proc_ptr) {
      lives_widget_set_sensitive(mainw->proc_ptr->cancel_button, TRUE);
      if (mainw->proc_ptr->stop_button)
        lives_widget_set_sensitive(mainw->proc_ptr->stop_button, TRUE);
      lives_widget_set_sensitive(mainw->proc_ptr->pause_button, TRUE);
      lives_widget_set_sensitive(mainw->proc_ptr->preview_button, TRUE);
    }
  }

  if (mainw->ce_thumbs) ce_thumbs_set_interactive(interactive);
  if (rte_window) rte_window_set_interactive(interactive);

  if (mainw->sense_state & LIVES_SENSE_STATE_INSENSITIZED) {
    if (!mainw->multitrack) desensitize();
    else mt_desensitise(mainw->multitrack);
  }
  if (mainw->sense_state & LIVES_SENSE_STATE_PROC_INSENSITIZED) {
    procw_desensitize();
  }
}


void fade_background(void) {
  if (palette->style == STYLE_PLAIN) {
    lives_label_set_text(LIVES_LABEL(mainw->banner), "            = <  L i V E S > =              ");
  }
  lives_frame_set_label(LIVES_FRAME(mainw->playframe), NULL);
  if (mainw->foreign) {
    lives_label_set_text(LIVES_LABEL(mainw->banner), _("    Press 'q' to stop recording.  DO NOT COVER THE PLAY WINDOW !   "));
    lives_widget_set_fg_color(mainw->banner, LIVES_WIDGET_STATE_NORMAL, &palette->banner_fade_text);
    lives_label_set_text(LIVES_LABEL(mainw->vps_label), ("                      "));
  } else {
    if (mainw->sep_win) {
      lives_widget_set_opacity(mainw->playframe, 0.);
    }
  }

  set_colours(&palette->normal_fore, &palette->fade_colour, &palette->menu_and_bars_fore, &palette->menu_and_bars,
              &palette->info_base, &palette->info_text);

  clear_widget_bg(mainw->play_image, mainw->play_surface);

  lives_frame_set_label(LIVES_FRAME(mainw->frame1), NULL);
  lives_frame_set_label(LIVES_FRAME(mainw->frame2), NULL);

  if (mainw->toy_type != LIVES_TOY_MAD_FRAMES || mainw->foreign) {
    lives_widget_hide(mainw->frame1);
    lives_widget_hide(mainw->frame2);
  }
  if (!mainw->foreign && prefs->show_tool) {
    lives_widget_set_no_show_all(mainw->tb_hbox, FALSE);
    lives_widget_show_all(mainw->tb_hbox);
    lives_widget_set_no_show_all(mainw->tb_hbox, TRUE);
  }

  lives_widget_hide(mainw->menu_hbox);
  lives_widget_hide(mainw->btoolbar);
  lives_widget_hide(mainw->framebar);
  lives_widget_hide(mainw->hbox3);
  lives_widget_hide(mainw->hruler);
  lives_widget_hide(mainw->hseparator);
  lives_widget_hide(mainw->sep_image);
  lives_widget_hide(mainw->eventbox2);
  if (prefs->show_msg_area) lives_widget_hide(mainw->message_box);

  if (!mainw->foreign) {
    lives_widget_show(mainw->t_forward);
    lives_widget_show(mainw->t_back);
    lives_widget_show(mainw->t_slower);
    lives_widget_show(mainw->t_faster);
    lives_widget_show(mainw->t_fullscreen);
    lives_widget_show(mainw->t_sepwin);
  }

  // since the hidden menu buttons are not activatable on some window managers
  // we need to remove the accelerators and add accelerator keys instead

  if (!stop_closure) {
    lives_widget_remove_accelerator(mainw->stop, mainw->accel_group, LIVES_KEY_q, (LiVESXModifierType)0);
    lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_q, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                              (stop_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(stop_callback), NULL, NULL)));

    if (!mainw->foreign) {
      // TODO - do these checks in the end functions
      lives_widget_remove_accelerator(mainw->record_perf, mainw->accel_group, LIVES_KEY_r, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_r, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (rec_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(rec_callback), NULL, NULL)));

      lives_widget_remove_accelerator(mainw->full_screen, mainw->accel_group, LIVES_KEY_f, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_f, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (fullscreen_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(fullscreen_callback), NULL, NULL)));

      lives_widget_remove_accelerator(mainw->showfct, mainw->accel_group, LIVES_KEY_h, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_h, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (showfct_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(showfct_callback), NULL, NULL)));

      lives_widget_remove_accelerator(mainw->sepwin, mainw->accel_group, LIVES_KEY_s, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_s, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (sepwin_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(sepwin_callback), NULL, NULL)));

      if (!CURRENT_CLIP_HAS_AUDIO || mainw->mute || mainw->loop_cont || is_realtime_aplayer(prefs->audio_player)) {
        lives_widget_remove_accelerator(mainw->loop_video, mainw->accel_group, LIVES_KEY_l, (LiVESXModifierType)0);
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_l, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  (loop_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(loop_callback), NULL, NULL)));

        lives_widget_remove_accelerator(mainw->loop_continue, mainw->accel_group, LIVES_KEY_o, (LiVESXModifierType)0);
        lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_o, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                  (loop_cont_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(loop_cont_callback), NULL, NULL)));

      }
      lives_widget_remove_accelerator(mainw->loop_ping_pong, mainw->accel_group, LIVES_KEY_g, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_g, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (ping_pong_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(ping_pong_callback), NULL, NULL)));
      lives_widget_remove_accelerator(mainw->mute_audio, mainw->accel_group, LIVES_KEY_z, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_z, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (mute_audio_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(mute_audio_callback), NULL, NULL)));
      lives_widget_remove_accelerator(mainw->dsize, mainw->accel_group, LIVES_KEY_d, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_d, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (dblsize_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(dblsize_callback), NULL, NULL)));
      lives_widget_remove_accelerator(mainw->fade, mainw->accel_group, LIVES_KEY_b, (LiVESXModifierType)0);
      lives_accel_group_connect(LIVES_ACCEL_GROUP(mainw->accel_group), LIVES_KEY_b, (LiVESXModifierType)0, (LiVESAccelFlags)0,
                                (fade_closure = lives_cclosure_new(LIVES_GUI_CALLBACK(fade_callback), NULL, NULL)));
    }
  }

  lives_widget_queue_draw(mainw->top_vbox);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
}


void unfade_background(void) {
  if (mainw->multitrack) return;

  if (palette->style == STYLE_PLAIN) {
    lives_label_set_text(LIVES_LABEL(mainw->banner), "   = <  L i V E S > =                            ");
  } else {
    lives_label_set_text(LIVES_LABEL(mainw->banner), "                                                ");
  }

  lives_frame_set_label(LIVES_FRAME(mainw->frame1), _("First Frame"));
  if (!mainw->preview) {
    lives_frame_set_label(LIVES_FRAME(mainw->playframe), _("Play"));
  } else {
    lives_frame_set_label(LIVES_FRAME(mainw->playframe), _("Preview"));
  }

  lives_frame_set_label(LIVES_FRAME(mainw->frame2), _("Last Frame"));

  if (!prefs->hide_framebar && !(!LIVES_IS_PLAYING && prefs->hfbwnp)) {
    lives_widget_show(mainw->framebar);
  }

  lives_widget_show_all(mainw->menu_hbox);
  lives_widget_show_all(mainw->hbox3);
  lives_widget_show(mainw->btoolbar);

  lives_widget_hide(mainw->tb_hbox);
  if (!mainw->double_size) {
    if (palette->style & STYLE_1) {
      lives_widget_show_all(mainw->sep_image);
    }
    lives_widget_show_all(mainw->hseparator);
    if (prefs->show_msg_area) lives_widget_show_all(mainw->message_box);
  }

  lives_widget_show_all(mainw->eventbox2);
  lives_widget_show_all(mainw->eventbox3);
  lives_widget_show_all(mainw->eventbox4);

  if (!CURRENT_CLIP_IS_VALID || !cfile->opening) {
    lives_widget_show(mainw->hruler);
  }

  if (CURRENT_CLIP_HAS_VIDEO && !mainw->sep_win) {
    lives_widget_show_all(mainw->playframe);
    lives_widget_set_opacity(mainw->playframe, 1.);
  }

  if (stop_closure && prefs->show_gui) {
    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), stop_closure);
    lives_widget_add_accelerator(mainw->stop, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_q, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), rec_closure);
    lives_widget_add_accelerator(mainw->record_perf, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_r, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), fullscreen_closure);
    lives_widget_add_accelerator(mainw->full_screen, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_f, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), sepwin_closure);
    lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_s, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), mute_audio_closure);
    lives_widget_add_accelerator(mainw->mute_audio, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_z, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), ping_pong_closure);
    lives_widget_add_accelerator(mainw->loop_ping_pong, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_g, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    if (!CURRENT_CLIP_HAS_AUDIO || mainw->mute || mainw->loop_cont || is_realtime_aplayer(prefs->audio_player)) {
      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), loop_closure);
      lives_widget_add_accelerator(mainw->loop_video, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                   LIVES_KEY_l, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), loop_cont_closure);
      lives_widget_add_accelerator(mainw->loop_continue, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                   LIVES_KEY_o, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);
      lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), showfct_closure);
    }
    lives_widget_add_accelerator(mainw->showfct, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_h, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), showsubs_closure);
    lives_widget_add_accelerator(mainw->showsubs, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_v, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), sepwin_closure);
    lives_widget_add_accelerator(mainw->sepwin, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_s, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), dblsize_closure);
    lives_widget_add_accelerator(mainw->dsize, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_d, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);

    lives_accel_group_disconnect(LIVES_ACCEL_GROUP(mainw->accel_group), fade_closure);
    lives_widget_add_accelerator(mainw->fade, LIVES_WIDGET_ACTIVATE_SIGNAL, mainw->accel_group,
                                 LIVES_KEY_b, (LiVESXModifierType)0, LIVES_ACCEL_VISIBLE);
    stop_closure = NULL;
  }

  if (mainw->double_size && !mainw->play_window) {
    resize(2.);
  } else {
    resize(1.);
  }

  set_colours(&palette->normal_fore, &palette->normal_back, &palette->menu_and_bars_fore, &palette->menu_and_bars,
              &palette->info_base, &palette->info_text);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
}

#define SCRN_BRDR 2.

void fullscreen_internal(void) {
  // resize for full screen, internal player, no separate window
  int width, height;

  if (!mainw->multitrack) {
    lives_widget_hide(mainw->framebar);

    // hide all except tb_hbox and pf_grid
    if (!mainw->faded) fade_background();

    // hide start and end frames in pf_grid
    lives_table_set_column_homogeneous(LIVES_TABLE(mainw->pf_grid), FALSE);

    lives_widget_hide(mainw->eventbox3);
    lives_widget_hide(mainw->eventbox4);

    lives_frame_set_label(LIVES_FRAME(mainw->playframe), NULL);

    if (prefs->show_msg_area) lives_widget_hide(mainw->message_box);

    //lives_widget_context_update();

    if (prefs->open_maximised) {
      lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
      lives_widget_queue_resize(LIVES_MAIN_WINDOW_WIDGET);
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
    }

    // try to get exact inner size of the main window
    lives_window_get_inner_size(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET), &width, &height);

    height -= SCRN_BRDR; // necessary, or screen expands too much (!?)

    // expand the inner box to fit this
    lives_widget_set_size_request(mainw->top_vbox, width, height);
    lives_widget_queue_resize(mainw->top_vbox);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);

    // this and pf_grid should be the only other widgets still visible
    if (prefs->show_tool) height -= lives_widget_get_allocation_height(mainw->tb_hbox);

    // image loading size in some cases
    mainw->ce_frame_width = width;
    mainw->ce_frame_height = height;

    if (!LIVES_IS_PLAYING) {
      set_drawing_area_from_pixbuf(mainw->play_image, NULL, mainw->play_surface);
    }

    // visible contents of pf_grid
    lives_widget_set_size_request(mainw->pl_eventbox, width, height);
    lives_widget_set_size_request(mainw->playframe, width, height);
    lives_widget_set_size_request(mainw->playarea, width, height);
    lives_widget_set_size_request(mainw->play_image, width, height);
    lives_widget_set_margin_left(mainw->playframe, 0);
    lives_widget_set_margin_right(mainw->playframe, 0);

    lives_widget_queue_resize(mainw->pf_grid);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
  } else {
    make_play_window();
  }
}


void set_preview_box_colours(void) {
  lives_widget_set_bg_color(mainw->preview_image, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_bg_color(mainw->preview_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mainw->preview_box, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  lives_widget_set_bg_color(mainw->preview_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  lives_widget_set_fg_color(mainw->preview_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
  set_child_colour(mainw->preview_box, TRUE);
}


void make_preview_box(void) {
  // create a box to show frames in, this will go in the sepwin when we are not playing
  LiVESWidget *eventbox;
  LiVESWidget *hbox_buttons;
  LiVESWidget *radiobutton_free;
  LiVESWidget *radiobutton_start;
  LiVESWidget *radiobutton_end;
  LiVESWidget *radiobutton_ptr;
  LiVESWidget *rewind_img;
  LiVESWidget *playsel_img;
  LiVESWidget *play_img;
  LiVESWidget *loop_img;

  LiVESSList *radiobutton_group = NULL;

  char *tmp, *tmp2;

  mainw->preview_box = lives_vbox_new(FALSE, 0);

  eventbox = lives_event_box_new();
  lives_widget_set_events(eventbox, LIVES_SCROLL_MASK | LIVES_SMOOTH_SCROLL_MASK);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_SCROLL_EVENT,
                            LIVES_GUI_CALLBACK(on_mouse_scroll), NULL);

  lives_box_pack_start(LIVES_BOX(mainw->preview_box), eventbox, TRUE, TRUE, 0);

  lives_signal_sync_connect(LIVES_GUI_OBJECT(eventbox), LIVES_WIDGET_BUTTON_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(frame_context), LIVES_INT_TO_POINTER(3));

  mainw->preview_image = lives_standard_drawing_area_new(LIVES_GUI_CALLBACK(expose_pim), &mainw->pi_surface);

  lives_container_add(LIVES_CONTAINER(eventbox), mainw->preview_image);
  lives_widget_set_app_paintable(mainw->preview_image, TRUE);

  if (mainw->play_window) {
    if (!CURRENT_CLIP_HAS_VIDEO) {
      if (mainw->imframe) {
        lives_widget_set_size_request(mainw->preview_image, lives_pixbuf_get_width(mainw->imframe),
                                      lives_pixbuf_get_height(mainw->imframe));
      }
    } else
      lives_widget_set_size_request(mainw->preview_image, MAX(mainw->pwidth, mainw->sepwin_minwidth), mainw->pheight);
  }

  lives_widget_set_hexpand(mainw->preview_box, TRUE);
  lives_widget_set_vexpand(mainw->preview_box, TRUE);

  mainw->preview_controls = lives_vbox_new(FALSE, 0);
  add_hsep_to_box(LIVES_BOX(mainw->preview_controls));

  lives_box_pack_start(LIVES_BOX(mainw->preview_box), mainw->preview_controls, FALSE, FALSE, 0);
  lives_widget_set_vexpand(mainw->preview_controls, FALSE);

  mainw->preview_hbox = lives_hbox_new(FALSE, 0);
  lives_widget_set_vexpand(mainw->preview_hbox, FALSE);
  lives_container_set_border_width(LIVES_CONTAINER(mainw->preview_hbox), 0);

  lives_widget_set_bg_color(mainw->preview_hbox, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  mainw->preview_spinbutton = lives_standard_spin_button_new(NULL, (CURRENT_CLIP_HAS_VIDEO) ? 1. : 0.,
                              (CURRENT_CLIP_HAS_VIDEO) ? 1. : 0.,
                              (CURRENT_CLIP_HAS_VIDEO) ? cfile->frames : 0., 1., 1., 0,
                              LIVES_BOX(mainw->preview_hbox), _("Frame number to preview"));

  mainw->preview_scale = lives_standard_hscale_new(lives_spin_button_get_adjustment(LIVES_SPIN_BUTTON(
                           mainw->preview_spinbutton)));

  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), mainw->preview_scale, FALSE, FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), mainw->preview_hbox, FALSE, FALSE, 0);

  lives_entry_set_width_chars(LIVES_ENTRY(mainw->preview_spinbutton), PREVSBWIDTHCHARS);

  radiobutton_free = lives_standard_radio_button_new((tmp = (_("_Free"))), &radiobutton_group,
                     LIVES_BOX(mainw->preview_hbox), (tmp2 = (_("Free choice of frame number"))));
  lives_free(tmp); lives_free(tmp2);

  radiobutton_start = lives_standard_radio_button_new((tmp = (_("_Start"))), &radiobutton_group,
                      LIVES_BOX(mainw->preview_hbox),
                      (tmp2 = (_("Frame number is linked to start frame"))));
  lives_free(tmp); lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_start), mainw->prv_link == PRV_START);

  radiobutton_end = lives_standard_radio_button_new((tmp = (_("_End"))), &radiobutton_group,
                    LIVES_BOX(mainw->preview_hbox),
                    (tmp2 = (_("Frame number is linked to end frame"))));
  lives_free(tmp); lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_end), mainw->prv_link == PRV_END);

  radiobutton_ptr = lives_standard_radio_button_new((tmp = (_("_Pointer"))), &radiobutton_group,
                    LIVES_BOX(mainw->preview_hbox),
                    (tmp2 = (_("Frame number is linked to playback pointer"))));
  lives_free(tmp); lives_free(tmp2);

  lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(radiobutton_ptr), mainw->prv_link == PRV_DEFAULT);

  add_hsep_to_box(LIVES_BOX(mainw->preview_controls));

  // buttons
  hbox_buttons = lives_hbox_new(FALSE, 0);
  lives_box_pack_start(LIVES_BOX(mainw->preview_controls), hbox_buttons, TRUE, TRUE, 0);

  rewind_img = lives_image_new_from_stock(LIVES_STOCK_MEDIA_REWIND, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  mainw->p_rewindbutton = lives_standard_button_new(DEF_BUTTON_WIDTH / 2, DEF_BUTTON_HEIGHT);
  lives_standard_button_set_image(LIVES_BUTTON(mainw->p_rewindbutton), rewind_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_rewindbutton, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(mainw->p_rewindbutton, _("Rewind"));
  lives_widget_set_sensitive(mainw->p_rewindbutton, CURRENT_CLIP_IS_VALID && cfile->pointer_time > 0.);
  lives_widget_set_focus_on_click(mainw->p_rewindbutton, FALSE);

  play_img = lives_image_new_from_stock(LIVES_STOCK_MEDIA_PLAY, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  mainw->p_playbutton = lives_standard_button_new(DEF_BUTTON_WIDTH / 2, DEF_BUTTON_HEIGHT);
  lives_standard_button_set_image(LIVES_BUTTON(mainw->p_playbutton), play_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_playbutton, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(mainw->p_playbutton, _("Play all"));
  lives_widget_set_focus_on_click(mainw->p_playbutton, FALSE);

  playsel_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_PLAY_SEL, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  mainw->p_playselbutton = lives_standard_button_new(DEF_BUTTON_WIDTH / 2, DEF_BUTTON_HEIGHT);
  lives_standard_button_set_image(LIVES_BUTTON(mainw->p_playselbutton), playsel_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_playselbutton, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(mainw->p_playselbutton, _("Play Selection"));
  lives_widget_set_sensitive(mainw->p_playselbutton, CURRENT_CLIP_IS_VALID && cfile->frames > 0);
  lives_widget_set_focus_on_click(mainw->p_playselbutton, FALSE);

  loop_img = lives_image_new_from_stock(LIVES_STOCK_LOOP, LIVES_ICON_SIZE_LARGE_TOOLBAR);
  if (!loop_img) loop_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_LOOP,
                              LIVES_ICON_SIZE_LARGE_TOOLBAR);
  mainw->p_loopbutton = lives_standard_button_new(DEF_BUTTON_WIDTH / 2, DEF_BUTTON_HEIGHT);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_loopbutton, TRUE, TRUE, 0);
  lives_widget_set_tooltip_text(mainw->p_loopbutton, _("Loop On/Off"));
  lives_widget_set_sensitive(mainw->p_loopbutton, TRUE);
  lives_widget_set_focus_on_click(mainw->p_loopbutton, FALSE);
  lives_widget_set_opacity(mainw->p_loopbutton, .75);

  lives_standard_button_set_image(LIVES_BUTTON(mainw->p_loopbutton), loop_img);

  mainw->p_mute_img = lives_image_new_from_stock(LIVES_LIVES_STOCK_VOLUME_MUTE,
                      LIVES_ICON_SIZE_LARGE_TOOLBAR);

  mainw->p_mutebutton = lives_standard_button_new(DEF_BUTTON_WIDTH / 2, DEF_BUTTON_HEIGHT);
  lives_standard_button_set_image(LIVES_BUTTON(mainw->p_mutebutton), mainw->p_mute_img);
  lives_box_pack_start(LIVES_BOX(hbox_buttons), mainw->p_mutebutton, TRUE, TRUE, 0);
  lives_widget_set_focus_on_click(mainw->p_mutebutton, FALSE);

  if (!mainw->mute) {
    lives_widget_set_tooltip_text(mainw->p_mutebutton, _("Mute the audio (z)"));
    lives_widget_set_opacity(mainw->p_mutebutton, .75);
  } else {
    lives_widget_set_tooltip_text(mainw->p_mutebutton, _("Unmute the audio (z)"));
    lives_widget_set_opacity(mainw->p_mutebutton, 1.);
  }

  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton_free), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_prv_link_toggled),
                            LIVES_INT_TO_POINTER(PRV_FREE));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton_start), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_prv_link_toggled),
                            LIVES_INT_TO_POINTER(PRV_START));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton_end), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_prv_link_toggled),
                            LIVES_INT_TO_POINTER(PRV_END));
  lives_signal_sync_connect(LIVES_GUI_OBJECT(radiobutton_ptr), LIVES_WIDGET_TOGGLED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_prv_link_toggled),
                            LIVES_INT_TO_POINTER(PRV_PTR));

  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_playbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playall_activate), NULL);
  lives_signal_connect(LIVES_GUI_OBJECT(mainw->p_playselbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_playsel_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->p_rewindbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_rewind_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->p_mutebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_mute_button_activate), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->p_loopbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                            LIVES_GUI_CALLBACK(on_loop_button_activate), NULL);

  mainw->preview_spin_func = lives_signal_sync_connect_after(LIVES_GUI_OBJECT(mainw->preview_spinbutton),
                             LIVES_WIDGET_VALUE_CHANGED_SIGNAL,
                             LIVES_GUI_CALLBACK(on_preview_spinbutton_changed), NULL);

  if (palette->style & STYLE_1) set_preview_box_colours();

  lives_widget_show_all(mainw->preview_box);
  lives_widget_hide(mainw->preview_controls);
  lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
}


void enable_record(void) {
  lives_menu_item_set_text(mainw->record_perf, _("Start _recording"), TRUE);
  lives_widget_set_sensitive(mainw->record_perf, TRUE);
}


void toggle_record(void) {
  lives_menu_item_set_text(mainw->record_perf, _("Stop _recording"), TRUE);
}


void disable_record(void) {
  lives_menu_item_set_text(mainw->record_perf, _("_Record Performance"), TRUE);
}


void play_window_set_title(void) {
  char *xtrabit;
  char *title = NULL;
  double sepwin_scale = 0.;

  if (CURRENT_CLIP_IS_VALID) {
    sepwin_scale = sqrt(mainw->pwidth * mainw->pwidth + mainw->pheight * mainw->pheight) /
                   sqrt(cfile->hsize * cfile->hsize + cfile->vsize * cfile->vsize);
  }
  if (mainw->multitrack) return;
  if (!mainw->play_window) return;

  if (!LIVES_IS_PLAYING && sepwin_scale > 0.)
    xtrabit = lives_strdup_printf(_(" (%.0f %% scale)"), sepwin_scale * 100.);
  else
    xtrabit = lives_strdup("");

  if (LIVES_IS_PLAYING) {
    if (mainw->vpp && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) && mainw->fs)
      lives_window_set_title(LIVES_WINDOW(mainw->play_window), _("Streaming"));
    else {
      title = lives_strdup_printf(_("Play Window%s"), xtrabit);
      lives_window_set_title(LIVES_WINDOW(mainw->play_window), title);
    }
  } else {
    title = lives_strdup_printf(_("Preview Window%s"), xtrabit);
    lives_window_set_title(LIVES_WINDOW(mainw->play_window), title);
  }

  if (title) lives_free(title);
  lives_free(xtrabit);
}


void resize_widgets_for_monitor(boolean do_get_play_times) {
  // resize widgets if we are aware that monitor resolution has changed
  // (or at least try our best...)
  LiVESList *list;
  int i, current_file = mainw->current_file;
  boolean need_mt = FALSE, fake_evlist;

  mainw->reconfig = TRUE;
  mainw->suppress_dprint = TRUE;
  mainw->idlemax = 0;
  lives_widget_context_update();

  get_monitors(TRUE);

  if (mainw->multitrack) {
    if (!mainw->multitrack->event_list) {
      /// create a fake event list with no events
      /// this prevents problems like the multitrack window trying to auto reload from disk
      weed_plant_t *event_list = weed_plant_new(WEED_PLANT_EVENT_LIST);
      weed_set_int_value(event_list, WEED_LEAF_WEED_EVENT_API_VERSION, WEED_EVENT_API_VERSION);
      weed_set_voidptr_value(event_list, WEED_LEAF_FIRST, NULL);
      weed_set_voidptr_value(event_list, WEED_LEAF_LAST, NULL);
      mainw->multitrack->event_list = event_list;
      fake_evlist = TRUE;
    }
    multitrack_delete(mainw->multitrack, FALSE);
    need_mt = TRUE;
  }

  if (LIVES_IS_WIDGET(mainw->top_vbox)) {
    lives_widget_destroy(mainw->top_vbox);
    lives_widget_context_update();
  }

  create_LiVES();

  for (i = 0; i < MAX_FX_CANDIDATE_TYPES; i++) {
    if (i == FX_CANDIDATE_AUDIO_VOL) continue;
    mainw->fx_candidates[i].delegate = -1;
    mainw->fx_candidates[i].list = NULL;
    mainw->fx_candidates[i].func = 0l;
    mainw->fx_candidates[i].rfx = NULL;
  }

  mainw->resize_menuitem = NULL;
  replace_with_delegates();

  show_lives();

  list = mainw->cliplist;
  mainw->cliplist = NULL;
  while (list) {
    mainw->current_file = LIVES_POINTER_TO_INT(list->data);
    add_to_clipmenu();
    list = list->next;
  }

  set_interactive(prefs->interactive);

  mainw->is_ready = TRUE;
  mainw->go_away = FALSE;

  if (!need_mt) {
    lives_widget_context_update();
    if (current_file != -1) switch_clip(1, current_file, TRUE);
    else {
      resize(1);
    }
    if (mainw->play_window) {
      resize_play_window();
    }
  } else {
    on_multitrack_activate(NULL, NULL);
    if (fake_evlist) {
      wipe_layout(mainw->multitrack);
    }
  }

  lives_widget_context_update();

  mainw->suppress_dprint = FALSE;
  d_print(_("GUI size changed to %d X %d\n"), GUI_SCREEN_WIDTH, GUI_SCREEN_HEIGHT);

  if (prefs->open_maximised) {
    lives_window_maximize(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  } else {
    lives_window_center(LIVES_WINDOW(LIVES_MAIN_WINDOW_WIDGET));
  }
  lives_widget_queue_resize(LIVES_MAIN_WINDOW_WIDGET);
  mainw->reconfig = FALSE;

  if (LIVES_IS_PLAYING) mainw->cancelled = CANCEL_ERROR;
}


static void _make_play_window(void) {
  //  separate window
  pb_added = FALSE;

  if (LIVES_IS_PLAYING) {
    unhide_cursor(lives_widget_get_xwindow(mainw->playarea));
  }

  if (mainw->play_window) {
    // this shouldn't ever happen
    kill_play_window();
  }

  mainw->play_window = lives_window_new(LIVES_WINDOW_TOPLEVEL);
  if (mainw->multitrack) lives_window_set_decorated(LIVES_WINDOW(mainw->play_window), FALSE);
  gtk_window_set_skip_taskbar_hint(LIVES_WINDOW(mainw->play_window), TRUE);
  gtk_window_set_skip_pager_hint(LIVES_WINDOW(mainw->play_window), TRUE);

  if (prefs->show_gui) {
    lives_window_set_transient_for(LIVES_WINDOW(mainw->play_window), get_transient_full());
  }
  lives_widget_set_events(mainw->play_window, LIVES_SCROLL_MASK | LIVES_SMOOTH_SCROLL_MASK |
                          LIVES_KEY_PRESS_MASK | LIVES_KEY_RELEASE_MASK);

  // cannot do this or it forces showing on the GUI monitor
  //gtk_window_set_position(LIVES_WINDOW(mainw->play_window),GTK_WIN_POS_CENTER_ALWAYS);

  if (!mainw->multitrack) lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), mainw->accel_group);
  else lives_window_add_accel_group(LIVES_WINDOW(mainw->play_window), mainw->multitrack->accel_group);

  if (palette->style & STYLE_1) {
    lives_widget_set_bg_color(mainw->play_window, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }

  if ((!LIVES_IS_PLAYING && prefs->show_gui) || (LIVES_IS_PLAYING && prefs->show_playwin)) {
    // show the window (so we can hide its cursor !), and get its xwin
    if (!(mainw->fs && LIVES_IS_PLAYING && mainw->vpp)) {
      lives_widget_show_all(mainw->play_window);
    }
    _resize_play_window();
  }

  if (!mainw->play_window) return;

  if (!mainw->preview_box) {
    // create the preview box that shows frames
    make_preview_box();
  }

  //lives_widget_show_all(mainw->play_window);
  lives_container_add(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
  lives_widget_object_ref(mainw->preview_box);
  pb_added = TRUE;

  if (mainw->is_processing && (CURRENT_CLIP_IS_VALID && !cfile->nopreview))
    lives_widget_set_tooltip_text(mainw->p_playbutton, _("Preview"));

  if (LIVES_IS_PLAYING) {
    lives_widget_hide(mainw->preview_controls);
  } else {
    if (mainw->is_processing && (mainw->prv_link == PRV_START || mainw->prv_link == PRV_END)) {
      // block spinbutton in play window
      lives_widget_set_sensitive(mainw->preview_spinbutton, FALSE);
    }
    if (CURRENT_CLIP_IS_VALID && cfile->is_loaded && prefs->show_gui) {
      lives_widget_set_no_show_all(mainw->preview_controls, FALSE);
      lives_widget_show_all(mainw->preview_box);
      lives_widget_show_now(mainw->preview_box);
      lives_widget_set_no_show_all(mainw->preview_controls, TRUE);
    }
    load_preview_image(FALSE);
  }

  lives_widget_set_tooltip_text(mainw->m_sepwinbutton, _("Hide Play Window"));

  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_DELETE_EVENT,
                            LIVES_GUI_CALLBACK(on_stop_activate_by_del), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_KEY_PRESS_EVENT,
                            LIVES_GUI_CALLBACK(key_press_or_release), NULL);
  lives_signal_sync_connect(LIVES_GUI_OBJECT(mainw->play_window), LIVES_WIDGET_KEY_RELEASE_EVENT,
                            LIVES_GUI_CALLBACK(key_press_or_release), NULL);

  lives_widget_set_sensitive(mainw->play_window, TRUE);
}

void make_play_window(void) {
  main_thread_execute((lives_funcptr_t)_make_play_window, 0, NULL, "");
}


LIVES_GLOBAL_INLINE boolean get_play_screen_size(int *opwidth, int *opheight) {
  // get the size of the screen / player in fullscreen / sepwin mode
  // returns TRUE if we span multiple monitors, FALSE for single monitor mode

  if (prefs->play_monitor == 0) {
    if (capable->nmonitors > 1) {
      // spread over all monitors
#if !GTK_CHECK_VERSION(3, 22, 0)
      *opwidth = lives_screen_get_width(mainw->mgeom[0].screen);
      *opheight = lives_screen_get_height(mainw->mgeom[0].screen);
#else
      /// TODO: no doubt this is wrong and should be done taking into account vertical monitor layouts as well
      *opheight = mainw->mgeom[0].height;
      *opwidth = 0;
      for (register int i = 0; i < capable->nmonitors; i++) {
        *opwidth += mainw->mgeom[i].width;
      }
#endif
      return TRUE;
    } else {
      // but we only have one...
      *opwidth = mainw->mgeom[0].phys_width;
      *opheight = mainw->mgeom[0].phys_height;
    }
  } else {
    // single monitor
    *opwidth = mainw->mgeom[prefs->play_monitor - 1].phys_width;
    *opheight = mainw->mgeom[prefs->play_monitor - 1].phys_height;
  }
  return FALSE;
}

#define TEST_CE_THUMBS 0

static void _resize_play_window(void) {
  int opwx, opwy, pmonitor = prefs->play_monitor;
  boolean fullscreen = TRUE;
  boolean ext_audio = FALSE;

  int width = -1, height = -1, nwidth, nheight = 0;
  int scr_width = GUI_SCREEN_WIDTH;
  int scr_height = GUI_SCREEN_HEIGHT;
  int scr_width_safety = SCR_WIDTH_SAFETY;
  int scr_height_safety = SCR_HEIGHT_SAFETY;
  int bx, by;

  uint64_t xwinid = 0;

  if (TEST_CE_THUMBS) {
    lives_widget_hide(mainw->play_window);
    start_ce_thumb_mode();
    return;
  }

#ifdef DEBUG_HANGS
  fullscreen = FALSE;
#endif

  if (!mainw->play_window) return;

  get_border_size(LIVES_MAIN_WINDOW_WIDGET, &bx, &by);

  scr_width_safety += 2 * bx;
  scr_height_safety += 2 * by;

  if (!LIVES_IS_PLAYING && !mainw->multitrack)
    lives_window_set_decorated(LIVES_WINDOW(mainw->play_window), TRUE);

  if (lives_widget_is_visible(mainw->play_window)) {
    width = lives_widget_get_allocation_width(mainw->play_window);
    height = lives_widget_get_allocation_height(mainw->play_window);
  }

  if ((!CURRENT_CLIP_IS_VALID || (cfile->frames == 0 && !mainw->multitrack) ||
       (!cfile->is_loaded && !mainw->preview && cfile->clip_type != CLIP_TYPE_GENERATOR)) ||
      (mainw->multitrack && mainw->playing_file < 1 && !mainw->preview)) {
    if (mainw->imframe) {
      mainw->pwidth = lives_pixbuf_get_width(mainw->imframe);
      mainw->pheight = lives_pixbuf_get_height(mainw->imframe);
    } else {
      if (!mainw->multitrack) {
        mainw->pwidth = DEF_FRAME_HSIZE;
        mainw->pheight = DEF_FRAME_VSIZE;
      }
    }
  } else {
    if (!mainw->multitrack) {
      mainw->pwidth = cfile->hsize;
      mainw->pheight = cfile->vsize;
    } else {
      mainw->pwidth = mainw->files[mainw->multitrack->render_file]->hsize;
      mainw->pheight = mainw->files[mainw->multitrack->render_file]->vsize;
    }
  }

  if ((mainw->double_size || mainw->multitrack) && (!mainw->fs || !LIVES_IS_PLAYING)) {
    // double size: maxspect to the screen size
    mainw->pwidth = cfile->hsize;
    mainw->pheight = cfile->vsize;
    if (LIVES_IS_PLAYING) {
      scr_width_safety >>= 1;
      scr_height_safety >>= 1;
    }
  }

  if (!mainw->fs || !LIVES_IS_PLAYING) {
    if (pmonitor == 0) {
      if ((((mainw->double_size || mainw->multitrack) && (!mainw->fs || !LIVES_IS_PLAYING))) ||
          (mainw->pwidth > scr_width - scr_width_safety ||
           mainw->pheight > scr_height - scr_height_safety)) {
        calc_maxspect(scr_width - scr_width_safety, scr_height - scr_height_safety, &mainw->pwidth, &mainw->pheight);
      }
    } else {
      if ((((mainw->double_size || mainw->multitrack) && (!mainw->fs || !LIVES_IS_PLAYING))) ||
          (mainw->pwidth > mainw->mgeom[pmonitor - 1].width - scr_width_safety ||
           mainw->pheight > mainw->mgeom[pmonitor - 1].height - scr_height_safety)) {
        calc_maxspect(mainw->mgeom[pmonitor - 1].width - scr_width_safety,
                      mainw->mgeom[pmonitor - 1].height - scr_height_safety,
                      &mainw->pwidth, &mainw->pheight);
      }
    }
  }

  if (LIVES_IS_PLAYING) {
    // fullscreen
    if (mainw->fs) {
      if (!lives_widget_is_visible(mainw->play_window)) {
        if (prefs->show_playwin) {
          lives_widget_show(mainw->play_window);
        }
        lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET);
        mainw->opwx = mainw->opwy = -1;
      } else {
        if (pmonitor == 0) {
          mainw->opwx = (scr_width - mainw->pwidth) / 2;
          mainw->opwy = (scr_height - mainw->pheight) / 2;
        } else {
          mainw->opwx = mainw->mgeom[pmonitor - 1].x + (mainw->mgeom[pmonitor - 1].width - mainw->pwidth) / 2;
          mainw->opwy = mainw->mgeom[pmonitor - 1].y + (mainw->mgeom[pmonitor - 1].height - mainw->pheight) / 2;
        }
      }

      get_play_screen_size(&mainw->pwidth, &mainw->pheight);

      if (lives_widget_is_visible(mainw->play_window)) {
        // store old position of window
        lives_window_get_position(LIVES_WINDOW(mainw->play_window), &opwx, &opwy);
        if (opwx * opwy > 0) {
          mainw->opwx = opwx;
          mainw->opwy = opwy;
        }
      }

      if (pmonitor == 0) {
        if (mainw->vpp && mainw->vpp->fwidth > 0) {
          lives_window_move(LIVES_WINDOW(mainw->play_window), (scr_width - mainw->vpp->fwidth) / 2,
                            (scr_height - mainw->vpp->fheight) / 2);
        } else lives_window_move(LIVES_WINDOW(mainw->play_window), 0, 0);
      } else {
        lives_window_set_monitor(LIVES_WINDOW(mainw->play_window), pmonitor - 1);
        if (mainw->vpp && mainw->vpp->fwidth > 0) {
          lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->mgeom[pmonitor - 1].x +
                            (mainw->mgeom[pmonitor - 1].width - mainw->vpp->fwidth) / 2,
                            mainw->mgeom[pmonitor - 1].y + (mainw->mgeom[pmonitor - 1].height - mainw->vpp->fheight) / 2);
        } else lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->mgeom[pmonitor - 1].x,
                                   mainw->mgeom[pmonitor - 1].y);
      }
      sched_yield();
      // leave this alone * !
      if (!(mainw->vpp && !(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY))) {
        mainw->ignore_screen_size = TRUE;
        /* if (prefs->show_desktop_panel && (capable->wm_caps.pan_annoy & ANNOY_DISPLAY) */
        /*     && (capable->wm_caps.pan_annoy & ANNOY_FS) && (capable->wm_caps.pan_res & RES_HIDE) && */
        /*     capable->wm_caps.pan_res & RESTYPE_ACTION) { */
        hide_desktop_panel();
        //0}
#if GTK_CHECK_VERSION(3, 18, 0)
        LiVESXWindow *xwin = lives_widget_get_xwindow(mainw->play_window);
        if (pmonitor == 0)
          gdk_window_set_fullscreen_mode(xwin, GDK_FULLSCREEN_ON_ALL_MONITORS);
        else
          gdk_window_set_fullscreen_mode(xwin, GDK_FULLSCREEN_ON_CURRENT_MONITOR);

        gtk_window_fullscreen_on_monitor(LIVES_WINDOW(mainw->play_window),
                                         mainw->mgeom[pmonitor - 1].screen, pmonitor - 1);
#else
        lives_widget_set_bg_color(mainw->play_window, LIVES_WIDGET_STATE_NORMAL, &palette->black);
        lives_window_fullscreen(LIVES_WINDOW(mainw->play_window));
#endif
        lives_window_set_decorated(LIVES_WINDOW(mainw->play_window), FALSE);
        lives_window_center(LIVES_WINDOW(mainw->play_window));
        lives_window_resize(LIVES_WINDOW(mainw->play_window), mainw->pwidth, mainw->pheight);
        lives_window_set_position(LIVES_WINDOW(mainw->play_window), LIVES_WIN_POS_NONE);
        lives_window_move(LIVES_WINDOW(mainw->play_window), 0, 0);
        lives_widget_queue_resize(mainw->play_window);
        lives_widget_context_update();
      }

      // init the playback plugin, unless the player cannot resize and there is a possibility of
      // wrongly sized frames (i.e. during a preview), or we are previewing and it's a remote display
      if (mainw->vpp && (!mainw->preview || ((mainw->vpp->capabilities & VPP_LOCAL_DISPLAY) &&
                                             (mainw->multitrack || (mainw->vpp->capabilities & VPP_CAN_RESIZE))))) {
        mainw->ptr_x = mainw->ptr_y = -1;
        if (pmonitor == 0) {
          // fullscreen playback on all screens (of first display)
          // get mouse position to warp it back after playback ends
          // in future we will handle multiple displays, so we will get the mouse device for the first screen of that display
          LiVESXDevice *device = mainw->mgeom[0].mouse_device;
#if GTK_CHECK_VERSION(3, 0, 0)
          if (device) {
#endif
            LiVESXScreen *screen;
            LiVESXDisplay *display = mainw->mgeom[0].disp;
            lives_display_get_pointer(device, display, &screen, &mainw->ptr_x, &mainw->ptr_y, NULL);
#if GTK_CHECK_VERSION(3, 0, 0)
          }
#endif
        }
        if (mainw->vpp->fheight > -1 && mainw->vpp->fwidth > -1) {
          // fixed o/p size for stream
          if (mainw->vpp->fwidth * mainw->vpp->fheight == 0) {
            mainw->vpp->fwidth = cfile->hsize;//DEF_VPP_HSIZE;
            mainw->vpp->fheight = cfile->vsize;//DEF_VPP_VSIZE;
          }
          if (!(mainw->vpp->capabilities & VPP_CAN_RESIZE)) {
            mainw->pwidth = mainw->vpp->fwidth;
            mainw->pheight = mainw->vpp->fheight;

            // * leave this alone !
            lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
            lives_window_resize(LIVES_WINDOW(mainw->play_window), mainw->pwidth, mainw->pheight);
            lives_widget_queue_resize(mainw->play_window);
          }
        }

        if (pmonitor != 0) {
          fullscreen = FALSE;
          if (mainw->play_window) {
            if (prefs->show_playwin) {
              xwinid = lives_widget_get_xwinid(mainw->play_window, "Unsupported display type for playback plugin");
              if (xwinid == -1) return;
            } else xwinid = -1;
          }
        }
        if (mainw->ext_playback) {
          lives_grab_remove(LIVES_MAIN_WINDOW_WIDGET);
#ifdef RT_AUDIO
          stop_audio_stream();
#endif
          pthread_mutex_lock(&mainw->vpp_stream_mutex);
          mainw->ext_audio = FALSE;
          pthread_mutex_unlock(&mainw->vpp_stream_mutex);

          if (mainw->vpp->exit_screen) {
            (*mainw->vpp->exit_screen)(mainw->ptr_x, mainw->ptr_y);
          }
          if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY && pmonitor == 0)
            lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window), FALSE);
        }

#ifdef RT_AUDIO
        if (mainw->vpp->audio_codec != AUDIO_CODEC_NONE && prefs->stream_audio_out) {
          start_audio_stream();
        } else {
          //clear_audio_stream();
        }
#endif

        if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY && pmonitor == 0)
          lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window), TRUE);

        if (mainw->vpp->init_audio && prefs->stream_audio_out) {
#ifdef HAVE_PULSE_AUDIO
          if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed) {
            if ((*mainw->vpp->init_audio)(mainw->pulsed->out_arate, mainw->pulsed->out_achans, mainw->vpp->extra_argc,
                                          mainw->vpp->extra_argv))
              ext_audio = TRUE;
          }
#endif
#ifdef ENABLE_JACK
          if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd) {
            if ((*mainw->vpp->init_audio)(mainw->jackd->sample_out_rate,
                                          mainw->jackd->num_output_channels, mainw->vpp->extra_argc,
                                          mainw->vpp->extra_argv))
              ext_audio = TRUE;
          }
#endif
        }

        if (!mainw->vpp->init_screen || ((*mainw->vpp->init_screen)
                                         (mainw->vpp->fwidth > 0 ? mainw->vpp->fwidth : mainw->pwidth,
                                          mainw->vpp->fheight > 0 ? mainw->vpp->fheight : mainw->pheight,
                                          fullscreen, xwinid, mainw->vpp->extra_argc, mainw->vpp->extra_argv))) {
          mainw->force_show = TRUE;
          mainw->ext_playback = TRUE;
          // the play window is still visible (in case it was 'always on top')
          // start key polling from ext plugin

          mainw->ext_audio = ext_audio; // cannot set this until after init_screen()

          if (mainw->vpp->capabilities & VPP_LOCAL_DISPLAY && (pmonitor == 0 || capable->nmonitors == 1)) {
            lives_grab_add(LIVES_MAIN_WINDOW_WIDGET);
          }
        } else if (mainw->vpp->init_screen) {
          LIVES_ERROR("Failed to start playback plugin");
        }
      }

      if (TEST_CE_THUMBS || (prefs->show_gui && prefs->ce_thumb_mode && prefs->play_monitor != widget_opts.monitor &&
                             prefs->play_monitor != 0 &&
                             capable->nmonitors > 1 && !mainw->multitrack)) {
        start_ce_thumb_mode();
      }
    } else {
      // NON fullscreen
      if (mainw->ce_thumbs) {
        end_ce_thumb_mode();
      }
      if (pmonitor > 0 && pmonitor != widget_opts.monitor + 1)
        lives_window_set_monitor(LIVES_WINDOW(mainw->play_window), pmonitor - 1);
      //lives_window_center(LIVES_WINDOW(mainw->play_window));
    }
    if (prefs->show_playwin) {
      lives_window_present(LIVES_WINDOW(mainw->play_window));
      lives_xwindow_raise(lives_widget_get_xwindow(mainw->play_window));
    }
  } else {
    // not playing
    if (mainw->fs && mainw->playing_file == -2 && mainw->sep_win && prefs->sepwin_type == SEPWIN_TYPE_STICKY) {
      if (mainw->ce_thumbs) {
        end_ce_thumb_mode();
      }
      if (!mainw->multitrack && mainw->opwx >= 0 && mainw->opwy >= 0) {
        // move window back to its old position after play
        if (pmonitor > 0) lives_window_set_monitor(LIVES_WINDOW(mainw->play_window),
              pmonitor - 1);
        lives_window_uncenter(LIVES_WINDOW(mainw->play_window));
        lives_window_move(LIVES_WINDOW(mainw->play_window), mainw->opwx, mainw->opwy);
      } else {
        if (pmonitor > 0) lives_window_set_monitor(LIVES_WINDOW(mainw->play_window), pmonitor - 1);
        lives_window_center(LIVES_WINDOW(mainw->play_window));
      }
    } else {
      if (pmonitor > 0) lives_window_set_monitor(LIVES_WINDOW(mainw->play_window), pmonitor - 1);
      lives_window_center(LIVES_WINDOW(mainw->play_window));
    }
    mainw->opwx = mainw->opwy = -1;
  }

  if (!LIVES_IS_PLAYING && (CURRENT_CLIP_IS_VALID && !cfile->opening)) {
    nheight = mainw->sepwin_minheight;
    if (mainw->pheight < MIN_SEPWIN_HEIGHT) nheight += MIN_SEPWIN_HEIGHT - mainw->pheight;
  }

  nwidth = mainw->pwidth;
  nheight += mainw->pheight;

  if (!(LIVES_IS_PLAYING && mainw->fs) && !mainw->double_size) {
    int xnwidth, xnheight;
    if (!LIVES_IS_PLAYING && CURRENT_CLIP_HAS_VIDEO && CURRENT_CLIP_IS_NORMAL)
      nwidth = MAX(mainw->pwidth, mainw->sepwin_minwidth);

    pmonitor = prefs->play_monitor;
    if (pmonitor == 0 || !LIVES_IS_PLAYING) {
      while (nwidth > GUI_SCREEN_WIDTH - scr_width_safety ||
             nheight > GUI_SCREEN_HEIGHT - scr_height_safety) {
        nheight <<= 3;
        nheight /= 10;
        nwidth <<= 3;
        nwidth /= 10;
      }
    } else {
      while (nwidth > mainw->mgeom[pmonitor - 1].width - scr_width_safety ||
             nheight > mainw->mgeom[pmonitor - 1].height - scr_height_safety) {
        nheight <<= 3;
        nheight /= 10;
        nwidth <<= 3;
        nwidth /= 10;
      }
    }
    xnwidth = nwidth;
    xnheight = nheight;
    calc_midspect(mainw->pwidth, mainw->pheight, &xnwidth, &xnheight);
    if (pmonitor == 0 || !LIVES_IS_PLAYING) {
      if (xnwidth <= GUI_SCREEN_WIDTH - scr_width_safety &&
          xnheight <= GUI_SCREEN_HEIGHT - scr_height_safety) {
        nwidth = xnwidth;
        nheight = xnheight;
      }
    } else {
      if (xnwidth <= mainw->mgeom[pmonitor - 1].width - scr_width_safety &&
          nheight <= mainw->mgeom[pmonitor - 1].height - scr_height_safety) {
        nwidth = xnwidth;
        nheight = xnheight;
      }
    }
  }

  if (!LIVES_IS_PLAYING || !mainw->fs) {
    lives_window_unfullscreen(LIVES_WINDOW(mainw->play_window));
    lives_window_resize(LIVES_WINDOW(mainw->play_window), nwidth, nheight);
    lives_window_center(LIVES_WINDOW(mainw->play_window));
    mainw->pwidth = nwidth;
    mainw->pheight = nheight;
  }

  if (!LIVES_IS_PLAYING) {
    if (pb_added && width != -1 && (width != nwidth || height != nheight) && mainw->preview_spinbutton) {
      load_preview_image(FALSE);
    }
    play_window_set_title();
  }
  clear_widget_bg(mainw->play_image, mainw->play_surface);
}


void resize_play_window(void) {
  main_thread_execute((lives_funcptr_t)_resize_play_window, 0, NULL, "");
}


static void _kill_play_window(void) {
  // plug our player back into internal window

  if (mainw->ce_thumbs) {
    end_ce_thumb_mode();
  }

  if (mainw->play_window) {
    if (mainw->preview_box && lives_widget_get_parent(mainw->preview_box)) {
      // preview_box is refed, so it will survive
      if (mainw->pi_surface) {
        /// invalid the preview window surface (IMPORTANT !)
        lives_painter_surface_destroy(mainw->pi_surface);
        mainw->pi_surface = NULL;
      }
      /// ref all the things and remove before destroying the window
      lives_widget_object_ref(mainw->preview_box);
      lives_widget_object_ref(mainw->preview_image);
      lives_container_remove(LIVES_CONTAINER(mainw->play_window), mainw->preview_box);
    }
    if (LIVES_IS_WINDOW(mainw->play_window)) {
      lives_widget_destroy(mainw->play_window);
    }
    mainw->play_window = NULL;
  }
  if ((!CURRENT_CLIP_IS_VALID || cfile->frames > 0) && !mainw->multitrack && LIVES_IS_PLAYING) {
    lives_widget_show_all(mainw->playframe);
    lives_widget_set_opacity(mainw->playframe, 1.);
  }
  lives_widget_set_tooltip_text(mainw->m_sepwinbutton, _("Show Play Window"));
}

void kill_play_window(void) {
  main_thread_execute((lives_funcptr_t)_kill_play_window, 0, NULL, "");
}


#define ASPECT_DIFF_LMT 0.01625f  // (fabs) ratio differences in aspect ratios within this limit considered irrelevant

/**
   @brief calculate sizes for letterboxing

    if the player can resize, then we only need to consider the aspect ratio.
    we will embed the image in a black rectangle to give it the same aspect ratio
    as the player; thus when it gets stretched to the player size the inner image will not be distorted
    so here we check: if we keep the same height, and then set the width to the player a.r, does it increase ?
    if so then our outer rectangle will be wider, otherwise it will be higher (or the same, in which case we dont do anything)
    - if either dimension ends up larger, then our outer rectangle is the player size, and we scale the inner image down so both
    width and height fit

    widths should be in pixels (not macropixels)
*/
void get_letterbox_sizes(int *pwidth, int *pheight, int *lb_width, int *lb_height, boolean player_can_upscale) {
  float frame_aspect, player_aspect;
  if (!player_can_upscale) {
    calc_maxspect(*pwidth, *pheight, lb_width, lb_height);
    return;
  }
  frame_aspect = (float) * lb_width / (float) * lb_height;
  player_aspect = (float) * pwidth / (float) * pheight;
  if (fabs(1. - frame_aspect / player_aspect) < ASPECT_DIFF_LMT) {
    if (*lb_width > *pwidth) *lb_width = *pwidth;
    if (*lb_height > *pheight) *lb_height = *pheight;
    if (*pwidth > *lb_width) *pwidth = *lb_width;
    if (*pheight > *lb_height) *pheight = *lb_height;
    return;
  }

  // *pwidth, *pheight are the outer dimensions, *lb_width, *lb_height are inner, widths are in pixels
  if (frame_aspect > player_aspect) {
    // width is relatively larger, so the height will need padding
    if (*lb_width > *pwidth) {
      /// inner frame needs scaling down
      *lb_width = *pwidth;
      *lb_height = (float) * lb_width / frame_aspect + .5;
      *lb_height = (*lb_height >> 1) << 1;
      return;
    } else {
      /// inner frame size OK, we will shrink wrap the outer frame
      *pwidth = *lb_width;
      *pheight = (float) * pwidth / player_aspect + .5;
      *pheight = (*pheight >> 1) << 1;
    }
    return;
  }
  if (*lb_height > *pheight) {
    *lb_height = *pheight;
    *lb_width = (float) * lb_height * frame_aspect + .5;;
    *lb_width = (*lb_width >> 2) << 2;
  } else {
    *pheight = *lb_height;
    *pwidth = (float) * pheight * player_aspect + .5;
    *pwidth = (*pwidth >> 2) << 2;
  }
}


void add_to_playframe(void) {
  if (LIVES_IS_PLAYING) lives_widget_show(mainw->playframe);

  if (!mainw->plug && !mainw->foreign) {
    mainw->plug = lives_event_box_new();
    lives_widget_set_app_paintable(mainw->plug, TRUE);
    lives_container_add(LIVES_CONTAINER(mainw->playarea), mainw->plug);
    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(mainw->plug, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }
    lives_widget_show(mainw->plug);
    lives_container_add(LIVES_CONTAINER(mainw->plug), mainw->play_image);
    lives_widget_set_halign(mainw->plug, LIVES_ALIGN_CENTER);
    lives_widget_set_halign(mainw->play_image, LIVES_ALIGN_CENTER);
    if (mainw->multitrack) {
      lives_widget_set_vexpand(mainw->play_image, TRUE); // centers it in mt
      lives_widget_set_hexpand(mainw->play_image, TRUE); // centers it in mt
    } else {
      lives_widget_set_vexpand(mainw->play_image, FALSE);
      lives_widget_set_hexpand(mainw->play_image, FALSE);
    }
  }
  resize(1);
  if (LIVES_IS_PLAYING) lives_widget_set_opacity(mainw->playframe, 1.);
  clear_widget_bg(mainw->play_image, mainw->play_surface);
}


LIVES_GLOBAL_INLINE void frame_size_update(void) {
  // update widgets when the frame size changes
  on_double_size_activate(NULL, LIVES_INT_TO_POINTER(1));
}


#define MAX_DISP_SETNAME_LEN 10

char *get_menu_name(lives_clip_t *sfile, boolean add_setname) {
  char *clipname;
  char *extra, *menuname;

  if (!sfile) return NULL;
  if (add_setname && sfile->was_in_set) {
    char *shortened_set_name;
    if (strlen(mainw->set_name) > MAX_DISP_SETNAME_LEN) {
      char *tmp = lives_strndup(mainw->set_name, MAX_DISP_SETNAME_LEN);
      shortened_set_name = lives_strdup_printf("%s...", tmp);
      lives_free(tmp);
    } else shortened_set_name = lives_strdup(mainw->set_name);
    extra = lives_strdup_printf(" (%s)", shortened_set_name);
    lives_free(shortened_set_name);
  } else extra = lives_strdup("");
  if (sfile->clip_type == CLIP_TYPE_FILE || sfile->clip_type == CLIP_TYPE_DISK)
    clipname = lives_path_get_basename(sfile->name);
  else clipname = lives_strdup(sfile->name);
  menuname = lives_strdup_printf("%s%s", clipname, extra);
  lives_free(extra);
  lives_free(clipname);
  return menuname;
}


void add_to_clipmenu(void) {
  // TODO - indicate "opening"
  char *fname = NULL;

#ifdef TEST_NOTIFY
  char *tmp, *detail;
#endif

#ifndef GTK_RADIO_MENU_BUG
  if (!CURRENT_CLIP_IS_VALID) return;
  widget_opts.mnemonic_label = FALSE;
  cfile->menuentry = lives_standard_radio_menu_item_new_with_label(mainw->clips_group, tmp = get_menu_name(cfile));
  lives_free(tmp);
  mainw->clips_group = lives_radio_menu_item_get_group(LIVES_RADIO_MENU_ITEM(cfile->menuentry));
#else
  widget_opts.mnemonic_label = FALSE;
  cfile->menuentry = lives_standard_check_menu_item_new_with_label(fname = get_menu_name(cfile, TRUE), FALSE);
  lives_check_menu_item_set_draw_as_radio(LIVES_CHECK_MENU_ITEM(cfile->menuentry), TRUE);
#endif

  if (!CURRENT_CLIP_IS_VALID) return;
  widget_opts.mnemonic_label = TRUE;
  lives_widget_show(cfile->menuentry);
  lives_container_add(LIVES_CONTAINER(mainw->clipsmenu), cfile->menuentry);

  lives_widget_set_sensitive(cfile->menuentry, TRUE);
  cfile->menuentry_func = lives_signal_sync_connect(LIVES_GUI_OBJECT(cfile->menuentry), LIVES_WIDGET_TOGGLED_SIGNAL,
                          LIVES_GUI_CALLBACK(switch_clip_activate), NULL);

  if (CURRENT_CLIP_IS_NORMAL) mainw->clips_available++;
  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_append(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  cfile->old_frames = cfile->frames;
  cfile->ratio_fps = check_for_ratio_fps(cfile->fps);

#ifdef TEST_NOTIFY
  detail = lives_strdup_printf(_("'LiVES opened the file' '%s'"), fname);
  tmp = lives_strdup_printf("notify-send %s", detail);
  lives_system(tmp, TRUE);
  lives_free(tmp);
  lives_free(detail);
#endif

  lives_freep((void **)&fname);
}


void remove_from_clipmenu(void) {
#ifndef GTK_RADIO_MENU_BUG
  LiVESList *list;
  int fileno;
#endif

#ifdef TEST_NOTIFY
  char *fname = get_menu_name(cfile);
  char *detail = lives_strdup_printf(_("'LiVES closed the file' '%s'"), fname);

  notify_user(detail);

  lives_free(fname);
  lives_free(detail);
#endif

  if (!CURRENT_CLIP_IS_VALID) return;
  lives_widget_unparent(cfile->menuentry);
  pthread_mutex_lock(&mainw->clip_list_mutex);
  mainw->cliplist = lives_list_remove(mainw->cliplist, LIVES_INT_TO_POINTER(mainw->current_file));
  pthread_mutex_unlock(&mainw->clip_list_mutex);
  if (CURRENT_CLIP_IS_NORMAL) {
    mainw->clips_available--;
    if (prefs->crash_recovery) rewrite_recovery_file();
  }

#ifndef GTK_RADIO_MENU_BUG
  list = mainw->cliplist;
  mainw->clips_group = NULL;

  while (list) {
    fileno = LIVES_POINTER_TO_INT(list->data);
    if (mainw->files[fileno] && mainw->files[fileno]->menuentrqy) {
      mainw->clips_group = lives_radio_menu_item_get_group(LIVES_RADIO_MENU_ITEM(mainw->files[fileno]->menuentry));
      break;
    }
  }
#endif
}

//////////////////////////////////////////////////////////////////////

// splash screen

void splash_init(void) {
  LiVESWidget *vbox, *hbox;
  LiVESWidget *splash_img;
  LiVESPixbuf *splash_pix;

  LiVESError *error = NULL;
  char *tmp = lives_strdup_printf("%s/%s/lives-splash.png", prefs->prefix_dir, THEME_DIR);
  lives_window_set_auto_startup_notification(FALSE);

  mainw->splash_window = lives_window_new(LIVES_WINDOW_TOPLEVEL);
  lives_window_set_decorated(LIVES_WINDOW(mainw->splash_window), FALSE);

  if (prefs->show_splash) {
#ifdef GUI_GTK
    gtk_window_set_type_hint(LIVES_WINDOW(mainw->splash_window), GDK_WINDOW_TYPE_HINT_SPLASHSCREEN);
#endif

    vbox = lives_vbox_new(FALSE, widget_opts.packing_height);
    lives_container_add(LIVES_CONTAINER(mainw->splash_window), vbox);
    lives_container_set_border_width(LIVES_CONTAINER(vbox), widget_opts.border_width * 4);

    if (palette->style & STYLE_1) {
      lives_widget_set_bg_color(mainw->splash_window, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
    }

    splash_pix = lives_pixbuf_new_from_file(tmp, &error);
    lives_free(tmp);

    splash_img = lives_image_new_from_pixbuf(splash_pix);

    lives_box_pack_start(LIVES_BOX(vbox), splash_img, TRUE, TRUE, 0);
    lives_widget_set_opacity(splash_img, .75);

    if (splash_pix) lives_widget_object_unref(splash_pix);

    mainw->splash_progress = lives_standard_progress_bar_new();

#ifdef PROGBAR_IS_ENTRY
    mainw->splash_label = mainw->splash_progress;
#else

    widget_opts.justify = LIVES_JUSTIFY_CENTER;
    mainw->splash_label = lives_standard_label_new("");
    widget_opts.justify = LIVES_JUSTIFY_DEFAULT;

    lives_box_pack_start(LIVES_BOX(vbox), mainw->splash_label, TRUE, TRUE, 4. * widget_opts.scale);
    lives_widget_set_valign(vbox, LIVES_ALIGN_END);
#endif

    lives_progress_bar_set_pulse_step(LIVES_PROGRESS_BAR(mainw->splash_progress), .01);

    if (palette->style & STYLE_1) {
      lives_widget_set_fg_color(mainw->splash_label, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
      lives_widget_set_fg_color(mainw->splash_progress, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    }

    hbox = lives_hbox_new(FALSE, widget_opts.packing_width);

    lives_box_pack_start(LIVES_BOX(hbox), mainw->splash_progress, TRUE, TRUE, widget_opts.packing_width * 2);

    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height * 2);

    lives_widget_show_all(mainw->splash_window);

    lives_window_set_monitor(LIVES_WINDOW(mainw->splash_window), widget_opts.monitor);

    lives_window_center(LIVES_WINDOW(mainw->splash_window));
    lives_window_present(LIVES_WINDOW(mainw->splash_window));
    if (mainw && LIVES_MAIN_WINDOW_WIDGET && prefs && prefs->startup_phase != 0)
      lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);

    lives_widget_context_update();
    lives_set_cursor_style(LIVES_CURSOR_BUSY, mainw->splash_window);
  } else {
    lives_widget_destroy(mainw->splash_window);
    mainw->splash_window = NULL;
  }

  lives_window_set_auto_startup_notification(TRUE);
}


void splash_msg(const char *msg, double pct) {
  if (mainw->foreign || !mainw->splash_window) return;

#ifdef PROGBAR_IS_ENTRY
  else {
    char *tmp = lives_strdup(msg);
    lives_chomp(tmp);
    lives_entry_set_text(LIVES_ENTRY(mainw->splash_label), tmp);
    lives_free(tmp);
  }
#else
  widget_opts.mnemonic_label = FALSE;
  lives_label_set_text(LIVES_LABEL(mainw->splash_label), msg);
  widget_opts.mnemonic_label = TRUE;
#endif

  lives_progress_bar_set_fraction(LIVES_PROGRESS_BAR(mainw->splash_progress), pct);

  lives_widget_queue_draw(mainw->splash_window);
  if (mainw && LIVES_MAIN_WINDOW_WIDGET && prefs && prefs->startup_phase != 0) lives_widget_hide(LIVES_MAIN_WINDOW_WIDGET);

  lives_widget_context_update();
}


void splash_end(void) {
  if (mainw->foreign) return;

  if (mainw->splash_window) {
    lives_set_cursor_style(LIVES_CURSOR_NORMAL, mainw->splash_window);
    lives_widget_destroy(mainw->splash_window);
  }

  mainw->threaded_dialog = FALSE;
  mainw->splash_window = NULL;

  if (prefs->startup_interface == STARTUP_MT && prefs->startup_phase == 0 && !mainw->multitrack) {
    on_multitrack_activate(NULL, NULL);
    mainw->is_ready = TRUE;
  }
}


void reset_message_area(void) {
  if (!prefs->show_msg_area || mainw->multitrack) return;
  if (!mainw->is_ready || !prefs->show_gui) return;
  // need to shrink the message_box then re-expand it after redrawing the widgets
  // otherwise the main window can expand beyond the bottom of the screen
  lives_widget_set_size_request(mainw->message_box, 1, 1);
  lives_widget_set_size_request(mainw->msg_area, 1, 1);
  lives_widget_set_size_request(mainw->msg_scrollbar, 1, 1);
}

