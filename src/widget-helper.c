// widget-helper.c
// LiVES
// (c) G. Finch 2012 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details



// The idea here is to replace toolkit specific functions with generic ones

// TODO - replace as much code in the other files with these functions as possible

// TODO - add for other toolkits, e.g. qt


#include "main.h"

// basic functions


boolean return_true (LiVESWidget *widget, LiVESEvent *event, LiVESObjectPtr user_data) {
  // event callback that just returns TRUE
  return TRUE;
}



LIVES_INLINE void lives_object_unref(LiVESObjectPtr object) {
#ifdef GUI_GTK
  g_object_unref(object);
#endif
}


LIVES_INLINE LiVESWidget *lives_dialog_get_content_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_content_area(GTK_DIALOG(dialog));
#else
  return GTK_DIALOG(dialog)->vbox;
#endif

#endif
}

LIVES_INLINE LiVESWidget *lives_dialog_get_action_area(LiVESDialog *dialog) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,14,0)
  return gtk_dialog_get_action_area(GTK_DIALOG(dialog));
#else
  return GTK_DIALOG(dialog)->vbox;
#endif

#endif
}




LIVES_INLINE LiVESPixbuf *lives_pixbuf_new(boolean has_alpha, int width, int height) {

#ifdef GUI_GTK
  // alpha fmt is RGBA post mult
  return gdk_pixbuf_new(GDK_COLORSPACE_RGB,has_alpha,8,width,height);
#endif
			
#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  enum fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  return new QImage(width, height, fmt);
#endif
}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_data (const unsigned char *buf, boolean has_alpha, int width, int height, 
						      int rowstride, LiVESPixbufDestroyNotify lives_free_buffer_fn, 
						      gpointer destroy_fn_data) {

#ifdef GUI_GTK
  return gdk_pixbuf_new_from_data ((const guchar *)buf, GDK_COLORSPACE_RGB, has_alpha, 8, width, height, rowstride, 
				   lives_free_buffer_fn, 
				   destroy_fn_data);
#endif


#ifdef GUI_QT
  // alpha fmt is ARGB32 premult
  enum fmt;
  if (!has_alpha) fmt=QImage::Format_RGB888;
  else {
    fmt=QImage::Format_ARGB32_Premultiplied;
    LIVES_WARN("Image fmt is ARGB pre");
  }
  // on destruct, we need to call lives_free_buffer_fn(uchar *pixels, gpointer destroy_fn_data)
  LIVES_ERROR("Need to set destructor fn for QImage");
  return new QImage((uchar *)buf, width, height, rowstride, fmt);
#endif

}



LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file(const char *filename, LiVESError **error) {
#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file(filename, error);
#endif

#ifdef GUI_QT
  QImage image = new QImage();
  if (!image.load(filename)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    ~image();
    return NULL;
  }
  return image;
}

#endif
}




LIVES_INLINE LiVESPixbuf *lives_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, 
							      boolean preserve_aspect_ratio,
							      LiVESError **error) {

#ifdef GUI_GTK
  return gdk_pixbuf_new_from_file_at_scale(filename, width, height, preserve_aspect_ratio, error);
#endif

#ifdef GUI_QT
  QImage image = QImage();
  QImage image2;
  if (!image.load(filename)) {
    // do something with error
    LIVES_WARN("QImage not loaded");
    return NULL;
  }
  if (preserve_aspect_ratio) asp=Qt::KeepAspectRatio;
  else asp=Qt::IgnoreAspectRatio;
  image2 = new image.scaled(width, height, asp,  Qt::SmoothTransformation);
  if (!image2) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return image2;
}

#endif
}



LIVES_INLINE int lives_pixbuf_get_rowstride(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_rowstride(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.bytesPerLine();
#endif
}


LIVES_INLINE int lives_pixbuf_get_width(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_width(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.width();
#endif
}


LIVES_INLINE int lives_pixbuf_get_height(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_height(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.height();
#endif
}


LIVES_INLINE int lives_pixbuf_get_n_channels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_n_channels(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.depth()>>3;
#endif
}



LIVES_INLINE unsigned char *lives_pixbuf_get_pixels(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.bits();
#endif
}


LIVES_INLINE const unsigned char *lives_pixbuf_get_pixels_readonly(const LiVESPixbuf *pixbuf) {

#ifdef GUI_GTK
  return (const guchar *)gdk_pixbuf_get_pixels(pixbuf);
#endif

#ifdef GUI_QT
  return (const uchar *)pixbuf.bits();
#endif
}



LIVES_INLINE boolean lives_pixbuf_get_has_alpha(const LiVESPixbuf *pixbuf) {
#ifdef GUI_GTK
  return gdk_pixbuf_get_has_alpha(pixbuf);
#endif

#ifdef GUI_QT
  return pixbuf.hasAlphaChannel();
#endif
}


LIVES_INLINE LiVESPixbuf *lives_pixbuf_scale_simple(const LiVESPixbuf *src, int dest_width, int dest_height, 
						    LiVESInterpType interp_type) {
  
#ifdef GUI_GTK
  return gdk_pixbuf_scale_simple(src, dest_width, dest_height, interp_type);
#endif


#ifdef GUI_QT
  QImage *image = new src.scaled(dest_width, dest_height, Qt::IgnoreAspectRatio,  interp_type);
  if (!image) {
    LIVES_WARN("QImage not scaled");
    return NULL;
  }

  return image;

#endif

}


LiVESWidget *lives_combo_new(void) {
  LiVESWidget *combo;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  combo = gtk_combo_box_text_new_with_entry ();
#else
  combo = gtk_combo_box_entry_new_text ();
#endif
#endif
  return combo;
}


void lives_combo_append_text(LiVESCombo *combo, const char *text) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),text);
#else
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),text);
#endif
#endif
}



void lives_combo_set_entry_text_column(LiVESCombo *combo, int column) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo),column);
#else
  gtk_combo_box_entry_set_text_column(GTK_COMBO_BOX_ENTRY(combo),column);
#endif
#endif
}


char *lives_combo_get_active_text(LiVESCombo *combo) {
  // return value should be freed
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,24,0)
  return gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
#else
  return gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
#endif
#endif
}



boolean lives_toggle_button_get_active(LiVESToggleButton *button) {
#ifdef GUI_GTK
  return gtk_toggle_button_get_active(button);
#endif
}

void lives_toggle_button_set_active(LiVESToggleButton *button, boolean active) {
#ifdef GUI_GTK
  gtk_toggle_button_set_active(button,active);
#endif
}



void lives_tooltips_set(LiVESWidget *widget, const char *tip_text) {
#ifdef GUI_GTK

#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(widget,tip_text);
#else
  GtkTooltips *tips;
  tips = gtk_tooltips_new ();
  gtk_tooltips_set_tip(tips,widget,tip_text,NULL);
#endif
#endif
}


LiVESSList *lives_radio_button_get_group(LiVESRadioButton *rbutton) {
#ifdef GUI_GTK
  return gtk_radio_button_get_group(rbutton);
#endif
}


LiVESWidget *lives_widget_get_parent(LiVESWidget *widget) {
#ifdef GUI_GTK
  return gtk_widget_get_parent(widget);
#endif
}


LiVESXWindow *lives_widget_get_xwindow(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,12,0)
  return gtk_widget_get_window(widget);
#else
  return GDK_WINDOW(widget->window);
#endif
#endif
}

void lives_widget_set_can_focus(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_focus(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_FOCUS);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_FOCUS);
#endif
#endif
}


void lives_widget_set_can_default(LiVESWidget *widget, boolean state) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  gtk_widget_set_can_default(widget,state);
#else
  if (state)
    GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  else
    GTK_WIDGET_UNSET_FLAGS (widget, GTK_CAN_DEFAULT);
#endif
#endif
}


boolean lives_widget_is_sensitive(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_is_sensitive(widget);
#else
  return GTK_WIDGET_IS_SENSITIVE (widget);
#endif
#endif
}

boolean lives_widget_is_visible(LiVESWidget *widget) {
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  return gtk_widget_get_visible(widget);
#else
  return GTK_WIDGET_VISIBLE (widget);
#endif
#endif
}


void lives_container_remove(LiVESContainer *container, LiVESWidget *widget) {
#ifdef GUI_GTK
  gtk_container_remove(container,widget);
#endif
}


void lives_ruler_set_range(LiVESRuler *ruler, double lower, double upper, double position, double max_size) {
#ifdef GUI_GTK
#if GTK_VERSION_3
  gtk_range_set_range(GTK_RANGE(ruler),upper,lower);
  gtk_range_set_value(GTK_RANGE(ruler),position);
#else
  gtk_ruler_set_range(ruler,lower,upper,position,max_size);
#endif
#endif
}


double lives_ruler_get_value(LiVESRuler *ruler) {
#ifdef GUI_GTK
#if GTK_VERSION_3
  return gtk_range_get_value(GTK_RANGE(ruler));
#else
  return ruler->position;
#endif
#endif
}

double lives_ruler_set_value(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_VERSION_3
  gtk_range_set_value(GTK_RANGE(ruler),value);
#else
  ruler->position=value;
#endif
#endif
  return value;
}


double lives_ruler_set_upper(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_VERSION_3
  gtk_adjustment_set_upper(gtk_range_get_adjustment(GTK_RANGE(ruler)),value);
#else
  ruler->upper=value;
#endif
#endif
  return value;
}


double lives_ruler_set_lower(LiVESRuler *ruler, double value) {
#ifdef GUI_GTK
#if GTK_VERSION_3
  gtk_adjustment_set_lower(gtk_range_get_adjustment(GTK_RANGE(ruler)),value);
#else
  ruler->lower=value;
#endif
#endif
  return value;
}


int lives_widget_get_allocation_x(LiVESWidget *widget) {
  int x=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  x=alloc.x;
#else
  x=widget->allocation.x;
#endif
#endif
  return x;
}


int lives_widget_get_allocation_y(LiVESWidget *widget) {
  int y=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  y=alloc.y;
#else
  y=widget->allocation.y;
#endif
#endif
  return y;
}

int lives_widget_get_allocation_width(LiVESWidget *widget) {
  int width=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  width=alloc.width;
#else
  width=widget->allocation.width;
#endif
#endif
  return width;
}

int lives_widget_get_allocation_height(LiVESWidget *widget) {
  int height=0;
#ifdef GUI_GTK
#if GTK_CHECK_VERSION(2,18,0)
  GtkAllocation alloc;
  gtk_widget_get_allocation(widget,&alloc);
  height=alloc.height;
#else
  height=widget->allocation.height;
#endif
#endif
  return height;
}


LiVESWidget *lives_bin_get_child(LiVESBin *bin) {
  LiVESWidget *child=NULL;
#ifdef GUI_GTK
  child=gtk_bin_get_child(bin);
#endif
  return child;
}



// compound functions


void lives_tooltips_copy(LiVESWidget *dest, LiVESWidget *source) {
#if GTK_CHECK_VERSION(2,12,0)
  gchar *text=gtk_widget_get_tooltip_text(source);
  gtk_widget_set_tooltip_text(dest,text);
  g_free(text);
#else
  GtkTooltipsData *td=gtk_tooltips_data_get(source);
  if (td==NULL) return;
  gtk_tooltips_set_tip (td->tooltips, dest, td->tip_text, td->tip_private);
#endif
}


void lives_combo_populate(LiVESCombo *combo, LiVESList *list) {
  // remove any current list
  int count;

  gtk_combo_box_set_active(combo,-1);
  count = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(combo),NULL);
  while (count-- > 0) gtk_combo_box_remove_text(combo,0);

  // add the new list
  while (list!=NULL) {
    lives_combo_append_text(LIVES_COMBO(combo),(const char *)list->data);
    list=list->next;
  }
}



LiVESWidget *lives_standard_label_new(const char *text) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK

  label=gtk_label_new(text);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), widget_opts.justify);
#endif

  return label;
}


LiVESWidget *lives_standard_label_new_with_mnemonic(const char *text, LiVESWidget *mnemonic_widget) {
  LiVESWidget *label=NULL;
#ifdef GUI_GTK

  label=gtk_label_new_with_mnemonic(text);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_label_set_justify (GTK_LABEL (label), widget_opts.justify);
  if (mnemonic_widget!=NULL) gtk_label_set_mnemonic_widget (GTK_LABEL(label),mnemonic_widget);
#endif

  return label;
}


LiVESWidget *lives_standard_check_button_new(const char *labeltext, boolean use_mnemonic, LiVESBox *box, 
					     const char *tooltip) {
  LiVESWidget *checkbutton=NULL;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;

  checkbutton = gtk_check_button_new ();
  if (tooltip!=NULL) lives_tooltips_set(checkbutton, tooltip);

  if (labeltext!=NULL) {
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,checkbutton);

    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,checkbutton);
    }
    else label=lives_standard_label_new (labeltext);

    gtk_container_add(GTK_CONTAINER(eventbox),label);
  }

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, W_PACKING_WIDTH);
  }
  
  gtk_box_set_homogeneous(GTK_BOX(hbox),FALSE);
  

  if (!widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);

  if (eventbox!=NULL) 
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      checkbutton);
  
  if (eventbox!=NULL&&mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_box_pack_start (GTK_BOX (hbox), checkbutton, FALSE, FALSE, W_PACKING_WIDTH);

  if (widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);

  lives_widget_set_can_focus_and_default(checkbutton);
#endif

  return checkbutton;
}




LiVESWidget *lives_standard_radio_button_new(const char *labeltext, boolean use_mnemonic, LiVESSList *rbgroup, 
					     LiVESBox *box, const char *tooltip) {
  LiVESWidget *radiobutton=NULL;

  // pack a themed check button into box



#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;

  radiobutton = gtk_radio_button_new (rbgroup);

  if (tooltip!=NULL) lives_tooltips_set(radiobutton, tooltip);

  lives_widget_set_can_focus_and_default(radiobutton);

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

  gtk_box_set_homogeneous(GTK_BOX(hbox),FALSE);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,radiobutton);
    }
    else label=lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,radiobutton);
    gtk_container_add(GTK_CONTAINER(eventbox),label);

    if (widget_opts.swap_label&&eventbox!=NULL)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, W_PACKING_WIDTH);

  if (eventbox!=NULL) {
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      radiobutton);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    if (!widget_opts.swap_label)
      gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

#endif

  return radiobutton;
}




LiVESWidget *lives_standard_spin_button_new(const char *labeltext, boolean use_mnemonic, double val, double min, 
					    double max, double step, double page, int dp, LiVESBox *box, 
					    const char *tooltip) {
  LiVESWidget *spinbutton=NULL;

  // pack a themed check button into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;
  LiVESObject *adj;

  char *txt;
  size_t maxlen;

  adj = gtk_adjustment_new (val, min, max, step, page, 0.);
  spinbutton = gtk_spin_button_new (GTK_ADJUSTMENT (adj), 1, dp);
  if (tooltip!=NULL) lives_tooltips_set(spinbutton, tooltip);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton),TRUE);
  txt=g_strdup_printf ("%d",(int)max);
  maxlen=strlen (txt);
  g_free (txt);
  txt=g_strdup_printf ("%d",(int)min);
  if (strlen (txt)>maxlen) maxlen=strlen (txt);
  g_free (txt);

  gtk_entry_set_width_chars (GTK_ENTRY (spinbutton),maxlen+dp<4?4:maxlen+dp+1);
  lives_widget_set_can_focus_and_default(spinbutton);
  gtk_entry_set_activates_default (GTK_ENTRY (spinbutton), TRUE);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),GTK_UPDATE_ALWAYS);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton),TRUE);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label=lives_standard_label_new_with_mnemonic (labeltext,spinbutton);
    }
    else label=lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,spinbutton);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

  if (!widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);

  gtk_box_pack_start (GTK_BOX (hbox), spinbutton, FALSE, FALSE, W_PACKING_WIDTH);

  if (widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);

#endif

  return spinbutton;
}






LiVESWidget *lives_standard_combo_new (const char *labeltext, boolean use_mnemonic, LiVESList *list, LiVESBox *box, 
				       const char *tooltip) {
  LiVESWidget *combo=NULL;

  // pack a themed combo box into box


#ifdef GUI_GTK
  LiVESWidget *eventbox=NULL;
  LiVESWidget *label;
  LiVESWidget *hbox;
  LiVESEntry *entry;

  combo=lives_combo_new();
  if (tooltip!=NULL) lives_tooltips_set(combo, tooltip);

  entry=(LiVESEntry *)lives_combo_get_entry(LIVES_COMBO(combo));

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,LIVES_WIDGET(entry));
    }
    else label = lives_standard_label_new (labeltext);
    
    eventbox=gtk_event_box_new();
    if (tooltip!=NULL) lives_tooltips_copy(eventbox,combo);
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    
    if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
  }

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);
  else {
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

  if (!widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);
  gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, TRUE, W_PACKING_WIDTH);
  if (widget_opts.swap_label&&eventbox!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, W_PACKING_WIDTH);

  gtk_editable_set_editable (GTK_EDITABLE(entry),FALSE);
  gtk_entry_set_activates_default(entry,TRUE);

  lives_combo_populate(LIVES_COMBO(combo),list);

  if (list!=NULL) gtk_combo_box_set_active(LIVES_COMBO(combo),0);
#endif

  return combo;
}


LiVESWidget *lives_standard_entry_new(const char *labeltext, boolean use_mnemonic, char *txt, int dispwidth, int maxchars, LiVESBox *box, 
					     const char *tooltip) {

  LiVESWidget *entry=NULL;

#ifdef GUI_GTK
  LiVESWidget *label=NULL;

  LiVESWidget *hbox;

  entry=gtk_entry_new();

  if (tooltip!=NULL) lives_tooltips_set(entry, tooltip);

  if (GTK_IS_HBOX(box)) hbox=GTK_WIDGET(box);

  else {
    hbox = gtk_hbox_new (TRUE, 0);
    gtk_box_pack_start (GTK_BOX (box), hbox, FALSE, FALSE, W_PACKING_WIDTH);
  }

  gtk_box_set_homogeneous(GTK_BOX(hbox),FALSE);

  gtk_entry_set_text (GTK_ENTRY (entry),txt);

  if (maxchars!=-1) gtk_entry_set_max_length(GTK_ENTRY (entry),maxchars);
  if (dispwidth!=-1) gtk_entry_set_width_chars (GTK_ENTRY (entry),dispwidth);
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);

  if (labeltext!=NULL) {
    if (use_mnemonic) {
      label = lives_standard_label_new_with_mnemonic (labeltext,entry);
    }
    else label = lives_standard_label_new (labeltext);

    if (tooltip!=NULL) lives_tooltips_copy(label,entry);
  }

  if (!widget_opts.swap_label&&label!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, W_PACKING_WIDTH);
  
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, W_PACKING_WIDTH);

  if (widget_opts.swap_label&&label!=NULL)
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, W_PACKING_WIDTH);
#endif

  return entry;
}



LiVESWidget *lives_standard_dialog_new(const char *title, boolean add_std_buttons) {
  LiVESWidget *dialog=NULL;

#ifdef GUI_GTK

  dialog = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (dialog), title);

  gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  if (prefs->gui_monitor!=0) {
    gtk_window_set_screen(GTK_WINDOW(dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
  }

  gtk_container_set_border_width (GTK_CONTAINER (dialog), W_BORDER_WIDTH);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER_ALWAYS);

  if (mainw!=NULL&&mainw->is_ready&&(palette->style&STYLE_1)) {
    gtk_widget_modify_bg(dialog, GTK_STATE_NORMAL, &palette->normal_back);
    gtk_dialog_set_has_separator(GTK_DIALOG(dialog),FALSE);
  }

  if (add_std_buttons) {
    GtkAccelGroup *accel_group=GTK_ACCEL_GROUP(gtk_accel_group_new ());
    GtkWidget *cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
    GtkWidget *okbutton = gtk_button_new_from_stock ("gtk-ok");

    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), cancelbutton, GTK_RESPONSE_CANCEL);

    gtk_widget_add_accelerator (cancelbutton, "activate", accel_group,
				LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);
    lives_widget_set_can_focus_and_default(cancelbutton);

    gtk_dialog_add_action_widget (GTK_DIALOG (dialog), okbutton, GTK_RESPONSE_OK);

    lives_widget_set_can_focus_and_default(okbutton);
    gtk_widget_grab_default (okbutton);
  }

  g_signal_connect (GTK_OBJECT (dialog), "delete_event",
                      G_CALLBACK (return_true),
                      NULL);

  gtk_widget_show(dialog);

  if (!widget_opts.non_modal)
    gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);


#endif

  return dialog;

}


LiVESWidget *lives_standard_hruler_new(void) {
  LiVESWidget *hruler=NULL;

#ifdef GUI_GTK
#if GTK_VERSION_3
  hruler=gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,NULL);
  gtk_scale_set_draw_value(GTK_SCALE(hruler),FALSE);
  gtk_scale_set_has_origin(GTK_SCALE(hruler),FALSE);
  gtk_scale_set_digits(GTK_SCALE(hruler),8);
#else
  hruler=gtk_hruler_new();
#endif
#endif

  return hruler;
}



// utils

void lives_widget_unparent(LiVESWidget *widget) {
  lives_container_remove(LIVES_CONTAINER(lives_widget_get_parent(widget)),widget);
}

boolean label_act_toggle (LiVESWidget *widget, LiVESEventButton *event, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active (togglebutton, !lives_toggle_button_get_active(togglebutton));
  return FALSE;
}

boolean widget_act_toggle (LiVESWidget *widget, LiVESToggleButton *togglebutton) {
  if (!lives_widget_is_sensitive(LIVES_WIDGET(togglebutton))) return FALSE;
  lives_toggle_button_set_active (togglebutton, TRUE);
  return FALSE;
}

LIVES_INLINE void toggle_button_toggle (LiVESToggleButton *tbutton) {
  if (lives_toggle_button_get_active(tbutton)) lives_toggle_button_set_active(tbutton,FALSE);
  else lives_toggle_button_set_active(tbutton,FALSE);
}


char *text_view_get_text(LiVESTextView *textview) {
  GtkTextIter siter,eiter;
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_get_start_iter(textbuf,&siter);
  gtk_text_buffer_get_end_iter(textbuf,&eiter);

  return gtk_text_buffer_get_text(textbuf,&siter,&eiter,TRUE);
}


void text_view_set_text(LiVESTextView *textview, const gchar *text) {
  GtkTextBuffer *textbuf=gtk_text_view_get_buffer (textview);
  gtk_text_buffer_set_text(textbuf,text,-1);
}


int get_box_child_index (LiVESBox *box, LiVESWidget *tchild) {
  GList *list=gtk_container_get_children(GTK_CONTAINER(box));
  GtkWidget *child;
  int i=0;

  while (list!=NULL) {
    child=(GtkWidget *)list->data;
    if (child==tchild) return i;
    list=list->next;
    i++;
  }
  return -1;
}


void adjustment_configure(LiVESAdjustment *adjustment,
		     double value,
		     double lower,
		     double upper,
		     double step_increment,
		     double page_increment,
		     double page_size) {
  g_object_freeze_notify (G_OBJECT(adjustment));

#if GTK_CHECK_VERSION(2,14,0)
  gtk_adjustment_configure(adjustment,value,lower,upper,step_increment,page_increment,page_size);
  g_object_thaw_notify (G_OBJECT(adjustment));
  return;
#else


  adjustment->upper=upper;
  adjustment->lower=lower;
  adjustment->value=value;
  adjustment->step_increment=step_increment;
  adjustment->page_increment=page_increment;
  adjustment->page_size=page_size;

  g_object_thaw_notify (G_OBJECT(adjustment));
#endif
}


void lives_set_cursor_style(lives_cursor_t cstyle, LiVESXWindow *window) {
  if (mainw->cursor!=NULL) gdk_cursor_unref(mainw->cursor);
  mainw->cursor=NULL;

  if (window==NULL) {
    if (mainw->multitrack==NULL&&mainw->is_ready) window=lives_widget_get_xwindow(mainw->LiVES);
    else if (mainw->multitrack!=NULL&&mainw->multitrack->is_ready) window=lives_widget_get_xwindow(mainw->multitrack->window);
    else return;
  }

  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    if (GDK_IS_WINDOW(window))
      gdk_window_set_cursor (window, NULL);
    return;
  case LIVES_CURSOR_BUSY:
    mainw->cursor=gdk_cursor_new(GDK_WATCH);
    if (GDK_IS_WINDOW(window))
      gdk_window_set_cursor (window, mainw->cursor);
    return;
  default:
    return;
  }
}




void hide_cursor(LiVESXWindow *window) {
  //make the cursor invisible in playback windows

#if GTK_CHECK_VERSION(3,0,0)
cairo_surface_t *s;
GdkPixbuf *pixbuf;

 if (hidden_cursor==NULL) {
   s = cairo_image_surface_create (CAIRO_FORMAT_A1, 1, 1);
   pixbuf = gdk_pixbuf_get_from_surface (s,
					 0, 0,
					 1, 1);
   
   cairo_surface_destroy (s);
   
   hidden_cursor = gdk_cursor_new_from_pixbuf (gdk_display_get_default(), pixbuf, 0, 0);
   
   g_object_unref (pixbuf);
 }
#else
  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (hidden_cursor==NULL) {
    source = gdk_bitmap_create_from_data (NULL, cursor_bits,
					  1, 1);
    mask = gdk_bitmap_create_from_data (NULL, cursormask_bits,
					1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap (source, mask, &fg, &bg, 0, 0);
    g_object_unref (source);
    g_object_unref (mask);
  }
#endif 
  if (GDK_IS_WINDOW(window))
    gdk_window_set_cursor (window, hidden_cursor);
}


void unhide_cursor(LiVESXWindow *window) {
  if (GDK_IS_WINDOW(window))
    gdk_window_set_cursor(window,NULL);
}


void get_border_size (LiVESWidget *win, int *bx, int *by) {
  GdkRectangle rect;
  gint wx,wy;
  gdk_window_get_frame_extents (lives_widget_get_xwindow (win),&rect);
  gdk_window_get_origin (lives_widget_get_xwindow (win), &wx, &wy);
  *bx=wx-rect.x;
  *by=wy-rect.y;
}




/*
 * Set active string to the combo box
 */
void lives_combo_set_active_string(LiVESCombo *combo, const char *active_str) {

#ifdef GUI_GTK
  gtk_entry_set_text(GTK_ENTRY(lives_bin_get_child(LIVES_BIN(combo))),active_str);
#endif

}

LiVESWidget *lives_combo_get_entry(LiVESCombo *widget) {
  return lives_bin_get_child(LIVES_BIN(widget));
}


void lives_widget_set_can_focus_and_default(LiVESWidget *widget) {
  lives_widget_set_can_focus(widget,TRUE);
  lives_widget_set_can_default(widget,TRUE);
}



void lives_general_button_clicked (LiVESButton *button, LiVESObjectPtr data_to_free) {
  // destroy the button top-level and free data

#ifdef GUI_GTK
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));

  if (data_to_free!=NULL) g_free(data_to_free);
#endif
}


boolean lives_general_delete_event(LiVESWidget *widget, LiVESEvent *event, LiVESObjectPtr data_to_free) {
#ifdef GUI_GTK
  gtk_widget_destroy(widget);

  if (data_to_free!=NULL) g_free(data_to_free);
#endif

  return TRUE;
}

void add_hsep_to_box (LiVESBox *box, boolean expand) {
#ifdef GUI_GTK
  GtkWidget *hseparator = gtk_hseparator_new ();
  gtk_box_pack_start (box, hseparator, expand, TRUE, 0);
  gtk_widget_show(hseparator);
#endif
}

void add_fill_to_box (LiVESBox *box) {
#ifdef GUI_GTK
  GtkWidget *blank_label = gtk_label_new ("");
  gtk_box_pack_start (box, blank_label, TRUE, TRUE, 0);
  gtk_widget_show(blank_label);
#endif
}
