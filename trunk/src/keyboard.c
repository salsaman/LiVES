// keyboard.c
// LiVES
// (c) G. Finch 2004 - 2012 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details


#include <gdk/gdkkeysyms.h>

#include "main.h"
#include "effects.h"
#include "callbacks.h"

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#endif

#define NEEDS_TRANSLATION 1<<15


#ifdef ENABLE_OSC
#include "omc-learn.h"
#endif

#ifdef ENABLE_OSC

static void handle_omc_events(void) {
  // check for external controller events

#ifdef OMC_MIDI_IMPL
  gint midi_check_rate;
  gboolean gotone;
#endif

  int i;

#ifdef OMC_JS_IMPL
    if (mainw->ext_cntl[EXT_CNTL_JS]) {
      gchar *string=js_mangle();
      if (string!=NULL) {
	omc_process_string(OMC_JS,string,FALSE,NULL);
	g_free(string);
	string=NULL;
      }
    }
#endif // OMC_JS_IMPL
    
#ifdef OMC_MIDI_IMPL
    midi_check_rate=prefs->midi_check_rate;
#ifdef ALSA_MIDI
    if (prefs->use_alsa_midi) midi_check_rate=1; // because we loop for events in midi_mangle()
#endif
    if (mainw->ext_cntl[EXT_CNTL_MIDI]) {
      do {
	gotone=FALSE;
	for (i=0;i<midi_check_rate;i++) {
	  gchar *string=midi_mangle();
	  if (string!=NULL) {
	    omc_process_string(OMC_MIDI,string,FALSE,NULL);
	    g_free(string);
	    string=NULL;
#ifdef ALSA_MIDI
	    if (prefs->use_alsa_midi) gotone=TRUE;
#endif
	  }
	}
      } while (gotone);
    }
#endif // OMC_MIDI_IMPL
}


#endif


gboolean ext_triggers_poll(gpointer data) {

  if (mainw->playing_file>-1) plugin_poll_keyboard(); ///< keyboard control during playback

  // check for external controller events
#ifdef ENABLE_JACK
#ifdef ENABLE_JACK_TRANSPORT
  if (mainw->jack_trans_poll) lives_jack_poll(); ///<   check for jack transport start
#endif
#endif

#ifdef ENABLE_OSC
  handle_omc_events(); ///< check for playback start triggered by js, MIDI, etc.
#endif

  /// if we have OSC we will poll it here,
#ifdef ENABLE_OSC
  if (prefs->osc_udp_started) lives_osc_poll(NULL);
#endif

  return TRUE;
}




GdkFilterReturn filter_func(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
  // filter events at X11 level and act on key press/release
  guint modifiers=0;

#ifdef USE_X11
  XEvent *xev=(XEvent *)xevent;
  if (xev->type<2||xev->type>3) return GDK_FILTER_CONTINUE;
  modifiers = (gtk_accelerator_get_default_mod_mask() & xev->xkey.state)|NEEDS_TRANSLATION;

  // key down
  if (xev->type==2) return pl_key_function(TRUE,xev->xkey.keycode,modifiers)?GDK_FILTER_REMOVE:GDK_FILTER_CONTINUE;

  // key up
  return pl_key_function(FALSE,xev->xkey.keycode,modifiers)?GDK_FILTER_REMOVE:GDK_FILTER_CONTINUE;
#else
  // windows uses MSGs
  PMSG msg=(PMSG)xevent;
  guint16 key;

#define KEY_DOWN(vk_code) ((GetAsyncKeyState(vk_code) & 0x8000) ? 1 : 0)

  if (msg->message!=WM_KEYDOWN && msg->message!=WM_KEYUP) return GDK_FILTER_CONTINUE;

  if (KEY_DOWN(VK_CONTROL))
    modifiers |= LIVES_CONTROL_MASK;
  
  if (KEY_DOWN(VK_MENU))
    modifiers |= LIVES_ALT_MASK;
  
  if (KEY_DOWN(VK_SHIFT))
    modifiers |= LIVES_SHIFT_MASK;

  key = (guint16)msg->wParam;
  
  switch (key) {
  case VK_LEFT: key=LIVES_KEY_Left; break;
  case VK_RIGHT: key=LIVES_KEY_Right; break;
  case VK_UP: key=LIVES_KEY_Up; break;
  case VK_DOWN: key=LIVES_KEY_Down; break;
  default: break;
  }
  
  if (msg->message==WM_KEYDOWN) return pl_key_function(TRUE,key,modifiers)?
				  GDK_FILTER_REMOVE:GDK_FILTER_CONTINUE;

  return pl_key_function(FALSE,key,modifiers)?GDK_FILTER_REMOVE:GDK_FILTER_CONTINUE;

#endif

  return GDK_FILTER_CONTINUE;
}




gboolean plugin_poll_keyboard (void) {
  static int last_kb_time=0,current_kb_time;

  // this is a function which should be called periodically during playback.
  // If a video playback plugin has control of the keyboard 
  // (e.g fullscreen video playback plugins)
  // it will be asked to send keycodes via pl_key_function

  // as of LiVES 1.1.0, this is now called 10 times faster to provide lower latency for
  // OSC and external controllers


  if (mainw->ext_keyboard) {
     //let plugin call pl_key_function itself, with any keycodes it has received
    if (mainw->vpp->send_keycodes!=NULL) (*mainw->vpp->send_keycodes)(pl_key_function);
   }
  
  current_kb_time=mainw->currticks*(1000/U_SEC_RATIO);

  // we also auto-repeat our cached keys
  if (cached_key&&current_kb_time-last_kb_time>KEY_RPT_INTERVAL*10) {
    last_kb_time=current_kb_time;
    gtk_accel_groups_activate (G_OBJECT (mainw->LiVES),(guint)cached_key, (GdkModifierType)cached_mod);
  }

  return TRUE;
}



gboolean pl_key_function (gboolean down, guint16 unicode, guint16 keymod) {
  // translate key events
  // plugins can also call this with a unicode key to pass key events to LiVES
  // (via a polling mechanism)

  // mask for ctrl and alt
  GdkModifierType state=(GdkModifierType)(keymod&(LIVES_CONTROL_MASK|LIVES_ALT_MASK));

  if (!down) {
    // up...
    if (keymod&NEEDS_TRANSLATION) {
      switch (unicode) {
	// some keys need translating when a modifier is held down
      case(key_left) :
      case (key_left2): if (cached_key==LIVES_KEY_Left) cached_key=0;return FALSE;
      case(key_right) : 
      case (key_right2): if (cached_key==LIVES_KEY_Right) cached_key=0;return FALSE;
      case(key_up)  :
      case (key_up2): if (cached_key==LIVES_KEY_Up) cached_key=0;return FALSE;
      case(key_down) : 
      case (key_down2): if (cached_key==LIVES_KEY_Down) cached_key=0;return FALSE;
      }
    }
    else {
      if (cached_key==unicode) cached_key=0;
    }
    return FALSE;
  }

  // translate hardware code into gdk keyval, and call any accelerators
  if (keymod&NEEDS_TRANSLATION) {
    switch (unicode) {
      // some keys need translating when a modifier is held down
    case (65) :unicode=LIVES_KEY_Space;break;
    case (22) :unicode=LIVES_KEY_BackSpace;break;
    case (36) :unicode=LIVES_KEY_Return;break;
    case (24) :unicode=LIVES_KEY_q;break;
    case (10) :unicode=LIVES_KEY_1;break;
    case (11) :unicode=LIVES_KEY_2;break;
    case (12) :unicode=LIVES_KEY_3;break;
    case (13) :unicode=LIVES_KEY_4;break;
    case (14) :unicode=LIVES_KEY_5;break;
    case (15) :unicode=LIVES_KEY_6;break;
    case (16) :unicode=LIVES_KEY_7;break;
    case (17) :unicode=LIVES_KEY_8;break;
    case (18) :unicode=LIVES_KEY_9;break;
    case (19) :unicode=LIVES_KEY_0;break;
    case (67) :unicode=LIVES_KEY_F1;break;
    case (68) :unicode=LIVES_KEY_F2;break;
    case (69) :unicode=LIVES_KEY_F3;break;
    case (70) :unicode=LIVES_KEY_F4;break;
    case (71) :unicode=LIVES_KEY_F5;break;
    case (72) :unicode=LIVES_KEY_F6;break;
    case (73) :unicode=LIVES_KEY_F7;break;
    case (74) :unicode=LIVES_KEY_F8;break;
    case (75) :unicode=LIVES_KEY_F9;break;
    case (76) :unicode=LIVES_KEY_F10;break;
    case (95) :unicode=LIVES_KEY_F11;break;
    case (96) :unicode=LIVES_KEY_F12;break;
    case (99) :
    case (112) :unicode=LIVES_KEY_Page_Up;break;
    case (105) :
    case (117) :unicode=LIVES_KEY_Page_Down;break;

      // auto repeat keys
    case(key_left) :
    case (key_left2): unicode=LIVES_KEY_Left;break;
    case(key_right) :
    case (key_right2): unicode=LIVES_KEY_Right;break;
    case(key_up)  :
    case (key_up2): unicode=LIVES_KEY_Up;break;
    case(key_down) :
    case (key_down2): unicode=LIVES_KEY_Down;break;

    }
  }

  
  if ((unicode==LIVES_KEY_Left||unicode==LIVES_KEY_Right||unicode==LIVES_KEY_Up||unicode==LIVES_KEY_Down)&&
      (keymod&LIVES_CONTROL_MASK)) {
    cached_key=unicode;
    cached_mod=LIVES_CONTROL_MASK;
  }

  if (mainw->rte_textparm!=NULL&&(keymod==0||keymod==LIVES_SHIFT_MASK||keymod==LIVES_LOCK_MASK)) {
    if (unicode==LIVES_KEY_Return||unicode==13) unicode='\n'; // CR
    if (unicode==LIVES_KEY_BackSpace) unicode=8; // bs
    if (unicode==LIVES_KEY_Tab||unicode==9) mainw->rte_textparm=NULL;
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
      if (mainw->record&&!mainw->record_paused&&mainw->playing_file>-1&&(prefs->rec_opts&REC_EFFECTS)) {
	// if we are recording, add this change to our event_list
	weed_plant_t *inst=weed_get_plantptr_value(mainw->rte_textparm,"host_instance",&error);
	int param_number=weed_get_int_value(mainw->rte_textparm,"host_idx",&error);
	rec_param_change(inst,param_number);
      }
      g_free(nval);
      return TRUE;
    }
  }

  if (mainw->ext_keyboard) {
    if (cached_key) return FALSE;
    if (mainw->multitrack==NULL) gtk_accel_groups_activate (G_OBJECT (mainw->LiVES),(guint)unicode,state);
    else gtk_accel_groups_activate (G_OBJECT (mainw->multitrack->window),(guint)unicode,state);
    if (!mainw->ext_keyboard) return TRUE; // if user switched out of ext_keyboard, do no further processing *
  }

  return FALSE;

  // * function was disabled so we must exit 
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

gboolean showsubs_callback (GtkAccelGroup *group, GObject *obj, guint keyval, GdkModifierType mod, gpointer user_data) {
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(mainw->showsubs),!prefs->show_subtitles);
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






