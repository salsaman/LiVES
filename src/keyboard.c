// keyboard.c
// LiVES
// (c) G. Finch 2004 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "effects.h"
#include "callbacks.h"

#include "../libweed/weed.h"
#include "../libweed/weed-host.h"


#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif


gboolean 
key_snooper (GtkWidget *widget, GdkEventKey *event, gpointer data) {
  // this gets called for every keypress - check for cached keys
  return pl_key_function ((event->type==GDK_KEY_PRESS),event->keyval,event->state);
}


gboolean 
plugin_poll_keyboard (gpointer data) {
  static gint osc_loop_count=0;

  int i;
  // this is a function which should be called periodically during playback.
  // If a video playback plugin has control of the keyboard 
  // (e.g fullscreen video playback plugins)
  // it will be asked to send keycodes via pl_key_function

  // TODO ** - check all of this


  if (osc_loop_count++>=prefs->osc_inv_latency-1) {
    osc_loop_count=0;
    if (mainw->ext_keyboard) {
      //let plugin call pl_key_function itself, with any keycodes it has received
      if (mainw->vpp->send_keycodes!=NULL) (*mainw->vpp->send_keycodes)(pl_key_function);
    }
    
    // we also auto-repeat our cached keys
    if (cached_key) gtk_accel_groups_activate (G_OBJECT (mainw->LiVES),(guint)cached_key,cached_mod);
  }

  // if we have OSC we will poll it here, as we seem to only be able to run 1 gtk_timeout at a time
#ifdef ENABLE_OSC
  if (prefs->osc_udp_started) lives_osc_poll(NULL);
#endif

#ifdef ENABLE_JACK
#ifdef ENABLE_JACK_TRANSPORT
  if (!mainw->is_processing) lives_jack_poll(NULL);
#endif
#endif

#ifdef ENABLE_OSC
#ifdef OMC_JS_IMPL
  if (mainw->ext_cntl[EXT_CNTL_JS]) {
    gchar *string=js_mangle();
    if (string!=NULL) {
      omc_process_string(OMC_JS,string,FALSE);
      g_free(string);
      string=NULL;
    }
  }
#endif

#ifdef OMC_MIDI_IMPL

  if (mainw->ext_cntl[EXT_CNTL_MIDI]) {
    for (i=0;i<prefs->midi_check_rate;i++) {
      gchar *string=midi_mangle();
      if (string!=NULL) {
	omc_process_string(OMC_MIDI,string,FALSE);
	g_free(string);
	string=NULL;
      }
    }
  }
#endif

#endif
  return TRUE;
}



gboolean pl_key_function (gboolean down, guint16 unicode, guint16 keymod) {
  // translate key events
  // plugins can call this with a unicode key to pass key events to LiVES

#define NEEDS_TRANSLATION 1<<15

  // mask for ctrl and alt
  GdkModifierType state=(keymod&(GDK_CONTROL_MASK|GDK_MOD1_MASK));

  if (!down) {
    // up...
    if (keymod&NEEDS_TRANSLATION) {
      switch (unicode) {
	// some keys need translating when a modifier is held down
      case(key_left) :
      case (key_left2): if (cached_key==GDK_Left) cached_key=0;return FALSE;
      case(key_right) : 
      case (key_right2): if (cached_key==GDK_Right) cached_key=0;return FALSE;
      case(key_up)  :
      case (key_up2): if (cached_key==GDK_Up) cached_key=0;return FALSE;
      case(key_down) : 
      case (key_down2): if (cached_key==GDK_Down) cached_key=0;return FALSE;
      }
    }
    else if (cached_key==unicode) cached_key=0;
    return FALSE;
  }

  // translate hardware code into gdk keyval, and call any accelerators
  if (keymod&NEEDS_TRANSLATION) {
    switch (unicode) {
      // some keys need translating when a modifier is held down
    case (65) :unicode=GDK_space;break;
    case (22) :unicode=GDK_BackSpace;break;
    case (36) :unicode=GDK_Return;break;
    case (24) :unicode=GDK_q;break;
    case (10) :unicode=GDK_1;break;
    case (11) :unicode=GDK_2;break;
    case (12) :unicode=GDK_3;break;
    case (13) :unicode=GDK_4;break;
    case (14) :unicode=GDK_5;break;
    case (15) :unicode=GDK_6;break;
    case (16) :unicode=GDK_7;break;
    case (17) :unicode=GDK_8;break;
    case (18) :unicode=GDK_9;break;
    case (19) :unicode=GDK_0;break;
    case (67) :unicode=GDK_F1;break;
    case (68) :unicode=GDK_F2;break;
    case (69) :unicode=GDK_F3;break;
    case (70) :unicode=GDK_F4;break;
    case (71) :unicode=GDK_F5;break;
    case (72) :unicode=GDK_F6;break;
    case (73) :unicode=GDK_F7;break;
    case (74) :unicode=GDK_F8;break;
    case (75) :unicode=GDK_F9;break;
    case (76) :unicode=GDK_F10;break;
    case (95) :unicode=GDK_F11;break;
    case (96) :unicode=GDK_F12;break;
    case (99) :
    case (112) :unicode=GDK_Page_Up;break;
    case (105) :
    case (117) :unicode=GDK_Page_Down;break;

      // auto repeat keys
    case(key_left) :
    case (key_left2): unicode=GDK_Left;break;
    case(key_right) :
    case (key_right2): unicode=GDK_Right;break;
    case(key_up)  :
    case (key_up2): unicode=GDK_Up;break;
    case(key_down) :
    case (key_down2): unicode=GDK_Down;break;

    }
  }

  if ((unicode==GDK_Left||unicode==GDK_Right||unicode==GDK_Up||unicode==GDK_Down)&&(keymod&GDK_CONTROL_MASK)) {
    cached_key=unicode;
    cached_mod=GDK_CONTROL_MASK;
  }

  if (mainw->rte_textparm!=NULL&&(keymod==0||keymod==GDK_SHIFT_MASK||keymod==GDK_LOCK_MASK)) {
    if (unicode==GDK_Return||unicode==13) unicode='\n'; // CR
    if (unicode==GDK_BackSpace) unicode=8; // bs
    if (unicode==GDK_Tab||unicode==9) mainw->rte_textparm=NULL;
    else if (unicode>0&&unicode<256) {
      int error;
      char *nval;
      char *cval=weed_get_string_value(mainw->rte_textparm,"value",&error);
      if (unicode==8&&strlen(cval)>0) { 
	memset(cval+strlen(cval)-1,0,1); // delete 1 char
	nval=g_strdup(cval);
      }
      else nval=g_strdup_printf("%s%c",cval,(unsigned char)unicode); // append 1 char
      weed_free(cval);
      weed_set_string_value(mainw->rte_textparm,"value",nval);
      g_free(nval);
      return TRUE;
    }
  }


  if (mainw->ext_keyboard) {
    if (cached_key) return FALSE;
    if (mainw->multitrack==NULL) gtk_accel_groups_activate (G_OBJECT (mainw->LiVES),(guint)unicode,state);
    else gtk_accel_groups_activate (G_OBJECT (mainw->multitrack->window),(guint)unicode,state);
  }

  return FALSE;
}


// key callback functions - ones which have keys and need wrappers


gboolean slower_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  
  on_slower_pressed (NULL,user_data);
  return TRUE;
}

gboolean faster_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  
  on_faster_pressed (NULL,user_data);
  return TRUE;
}

gboolean skip_back_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  
  on_back_pressed (NULL,user_data);
  return TRUE;
}

gboolean skip_forward_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  
  on_forward_pressed (NULL,user_data);
  return TRUE;
}

gboolean stop_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_stop_activate (NULL,NULL);
  return TRUE;
}

gboolean fullscreen_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_full_screen_pressed (NULL,NULL);
  return TRUE;
}

gboolean sepwin_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_sepwin_pressed (NULL,NULL);
  return TRUE;
}

gboolean loop_cont_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_loop_button_activate (NULL,NULL);
  return TRUE;
}

gboolean ping_pong_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_ping_pong_activate (NULL,NULL);
  return TRUE;
}

gboolean fade_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_fade_pressed (NULL,NULL);
  return TRUE;
}

gboolean showfct_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->showfct),!prefs->show_framecount);
  return TRUE;
}

gboolean loop_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->loop_video),!mainw->loop);
  return TRUE;
}

gboolean dblsize_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  on_double_size_pressed (NULL,NULL);
  return TRUE;
}

gboolean rec_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->record_perf),!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (mainw->record_perf)));
  return TRUE;
}






