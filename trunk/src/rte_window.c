// rte_window.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2010
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-host.h"
#include "weed/weed-effects.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-effects.h"
#endif

#include "main.h"

#include "support.h"
#include "rte_window.h"
#include "effects.h"
#include "paramwindow.h"

#define RTE_INFO_WIDTH 350
#define RTE_INFO_HEIGHT 200

static GtkWidget **key_checks;
static GtkWidget **key_grabs;
static GtkWidget **mode_radios;
static GtkWidget **combo_entries;
static GtkWidget *dummy_radio;
static GtkWidget **nlabels;
static GtkWidget **type_labels;
static GtkWidget **param_buttons;
static GtkWidget **clear_buttons;
static GtkWidget **info_buttons;
static GtkWidget *clear_all_button;
static GtkWidget *save_keymap_button;
static GtkWidget *load_keymap_button;

static gulong *ch_fns;
static gulong *gr_fns;
static gulong *mode_ra_fns;

static gint keyw=-1,modew=-1;

static GList *hash_list;
static GList *name_list;
static GList *name_type_list;


//////////////////////////////////////////////////////////////////////////////


void type_label_set_text (gint key, gint mode) {
  int modes=rte_getmodespk();
  int idx=key*modes+mode;
  gchar *type=rte_keymode_get_type(key+1,mode);

  if (strlen(type)) {
    gtk_label_set_text (GTK_LABEL(type_labels[idx]),g_strdup_printf(_("Type: %s"),type));
    gtk_widget_set_sensitive(info_buttons[idx],TRUE);
    gtk_widget_set_sensitive(clear_buttons[idx],TRUE);
    gtk_widget_set_sensitive(mode_radios[idx],TRUE);
    gtk_widget_set_sensitive(nlabels[idx],TRUE);
    gtk_widget_set_sensitive(type_labels[idx],TRUE);
  }
  else {
    gtk_widget_set_sensitive(info_buttons[idx],FALSE);
    gtk_widget_set_sensitive(clear_buttons[idx],FALSE);
    gtk_widget_set_sensitive(mode_radios[idx],FALSE);
    gtk_widget_set_sensitive(nlabels[idx],FALSE);
    gtk_widget_set_sensitive(type_labels[idx],FALSE);
  }
  g_free(type);
}


void on_rtei_ok_clicked (GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}


gboolean on_clear_all_clicked (GtkButton *button, gpointer user_data) {
  int modes=rte_getmodespk();
  int i,j;

  mainw->error=FALSE;

  // prompt for "are you sure ?"
  if (user_data!=NULL) if (!do_warning_dialog_with_check_transient((_("\n\nUnbind all effects from all keys/modes.\n\nAre you sure ?\n\n")),0,GTK_WINDOW(rte_window))) {
      mainw->error=TRUE;
      return FALSE;
    }

  for (i=0;i<prefs->rte_keys_virtual;i++) {
    if (rte_window!=NULL) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(key_checks[i]),FALSE);
    for (j=modes-1;j>=0;j--) {
      weed_delete_effectkey (i+1,j);
      if (rte_window!=NULL) {
	gtk_entry_set_text (GTK_ENTRY(combo_entries[i*modes+j]),"");
	type_label_set_text(i,j);
      }
    }
  }

  if (button!=NULL) gtk_widget_set_sensitive (GTK_WIDGET(button), FALSE);

  return FALSE;
}


static void save_keymap2_file(gchar *kfname) {
  int i,j;
  int slen;
  int version=1;
  int modes=rte_getmodespk();
  int kfd=creat(kfname,S_IRUSR|S_IWUSR);
  gchar *msg;
  gchar *hashname;

  if (kfd==-1) {
    msg=g_strdup_printf (_("\n\nUnable to write keymap file\n%s\nError code %d\n"),kfname,errno);
    do_error_dialog_with_check_transient (msg,FALSE,0,GTK_WINDOW(rte_window));
    g_free (msg);
    d_print_failed();
    return;
  }

  dummyvar=write(kfd,&version,sizint);

  for (i=1;i<=prefs->rte_keys_virtual;i++) {
    for (j=0;j<modes;j++) {
      if (rte_keymode_valid(i,j,TRUE)) {
	dummyvar=write(kfd,&i,sizint);
	hashname=g_strdup_printf("Weed%s",make_weed_hashname(rte_keymode_get_filter_idx(i,j),TRUE));
	slen=strlen(hashname);
	dummyvar=write(kfd,&slen,sizint);
	dummyvar=write(kfd,hashname,slen);
	g_free(hashname);
	write_key_defaults(kfd,i-1,j);
      }
    }
  }
  close(kfd);
}


gboolean on_save_keymap_clicked (GtkButton *button, gpointer user_data) {
  // quick and dirty implementation
  int modes=rte_getmodespk();
  int i,j;
  FILE *kfile;
  gchar *msg,*tmp;
  GList *list=NULL;
  gboolean update=FALSE;

  gchar *keymap_file=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap",NULL);
  gchar *keymap_file2=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap2",NULL);

  if (button!=NULL) {
    if (!do_warning_dialog_with_check_transient((_("\n\nClick 'OK' to save this keymap as your default\n\n")),0,GTK_WINDOW(rte_window))) {
      g_free(keymap_file2);
      g_free(keymap_file);
      return FALSE;
    }
    d_print ((tmp=g_strdup_printf(_("Saving keymap to %s\n"),keymap_file)));
    g_free(tmp);
  }
  else {
    update=TRUE;
    list=(GList *)user_data;
    if (list==NULL) return FALSE;
    d_print ((tmp=g_strdup_printf(_("\nUpdating keymap file %s..."),keymap_file)));
    g_free(tmp);
  }

  if (!(kfile=fopen(keymap_file,"w"))) {
    if (!update) {
      msg=g_strdup_printf (_("\n\nUnable to write keymap file\n%s\nError code %d\n"),keymap_file,errno);
      do_error_dialog_with_check_transient (msg,FALSE,0,GTK_WINDOW(rte_window));
      g_free (msg);
      d_print_failed();
    }
    g_free(keymap_file2);
    g_free(keymap_file);
    return FALSE;
  }

  fputs("LiVES keymap file version 4\n",kfile);

  if (!update) {
    for (i=1;i<=prefs->rte_keys_virtual;i++) {
      for (j=0;j<modes;j++) {
	if (rte_keymode_valid(i,j,TRUE)) {
	  fputs(g_strdup_printf("%d|Weed%s\n",i,make_weed_hashname(rte_keymode_get_filter_idx(i,j),TRUE)),kfile);
	}
      }
    }
  }
  else {
    for (i=0;i<g_list_length(list);i++) {
      fputs(g_list_nth_data(list,i),kfile);
    }
  }

  fclose (kfile);

  // if we have default values, save as newer style
  if (has_key_defaults()) {
    save_keymap2_file(keymap_file2);
  }
  else unlink(keymap_file2);

  g_free(keymap_file2);
  g_free(keymap_file);

  d_print_done();

  return FALSE;
}




void on_save_rte_defs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  int fd,i;
  gint numfx;
  gchar *msg;

  if (prefs->fxdefsfile==NULL) {
    prefs->fxdefsfile=g_strdup_printf("%s/%sfxdefs",capable->home_dir,LIVES_CONFIG_DIR);
  }

  if (prefs->fxsizesfile==NULL) {
    prefs->fxsizesfile=g_strdup_printf("%s/%sfxsizes",capable->home_dir,LIVES_CONFIG_DIR);
  }

  msg=g_strdup_printf(_("Saving real time effect defaults to %s..."),prefs->fxdefsfile);
  d_print(msg);
  g_free(msg);

  if ((fd=open(prefs->fxdefsfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
    msg=g_strdup_printf (_("\n\nUnable to write defaults file\n%s\nError code %d\n"),prefs->fxdefsfile,errno);
    do_error_dialog (msg);
    g_free (msg);
    d_print_failed();
    return;
  }
  
  msg=g_strdup("LiVES filter defaults file version 1.1\n");
  
  dummyvar=write(fd,msg,strlen(msg));
  g_free(msg);

  numfx=rte_get_numfilters();

  for (i=0;i<numfx;i++) write_filter_defaults(fd,i);

  close(fd);

  if ((fd=open(prefs->fxsizesfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
    msg=g_strdup_printf (_("\n\nUnable to write default sizes file\n%s\nError code %d\n"),prefs->fxsizesfile,errno);
    do_error_dialog (msg);
    g_free (msg);
    d_print_failed();
    return;
  }

  msg=g_strdup("LiVES generator default sizes file version 2\n");
  
  dummyvar=write(fd,msg,strlen(msg));
  g_free(msg);

  for (i=0;i<numfx;i++) write_generator_sizes(fd,i);

  close(fd);

  d_print_done();

  return;
}



void load_rte_defs (void) {
  int fd;
  gchar *msg,*msg2;
  void *buf;
  ssize_t bytes;

  if (prefs->fxdefsfile==NULL) {
    prefs->fxdefsfile=g_strdup_printf("%s/%sfxdefs",capable->home_dir,LIVES_CONFIG_DIR);
  }

  if (g_file_test(prefs->fxdefsfile,G_FILE_TEST_EXISTS)) {
    if ((fd=open(prefs->fxdefsfile,O_RDONLY))==-1) {
      msg2=g_strdup_printf (_("Unable to read defaults file\n%s\nError code %d\n"),prefs->fxdefsfile,errno);
      do_error_dialog (msg2);
      g_free (msg2);
    }
    else {
      msg2=g_strdup_printf(_("Loading real time effect defaults from %s..."),prefs->fxdefsfile);
      d_print(msg2);
      g_free(msg2);
      
      msg=g_strdup("LiVES filter defaults file version 1.1\n");
      buf=g_malloc(strlen(msg));
      bytes=read(fd,buf,strlen(msg));
      
      if (bytes==strlen(msg)&&!strncmp(buf,msg,strlen(msg))) {
	read_filter_defaults(fd);
	d_print_done();
      }
      else d_print_file_error_failed();
      
      close(fd);
      
      g_free(buf);
      g_free(msg);
    }
  }

  if (prefs->fxsizesfile==NULL) {
    prefs->fxsizesfile=g_strdup_printf("%s/%sfxsizes",capable->home_dir,LIVES_CONFIG_DIR);
  }

  if (g_file_test(prefs->fxsizesfile,G_FILE_TEST_EXISTS)) {
    if ((fd=open(prefs->fxsizesfile,O_RDONLY))==-1) {
      msg=g_strdup_printf (_("\n\nUnable to read sizes file\n%s\nError code %d\n"),prefs->fxsizesfile,errno);
      do_error_dialog (msg);
      g_free (msg);
      return;
    }
    
    msg2=g_strdup_printf(_("Loading generator default sizes from %s..."),prefs->fxsizesfile);
    d_print(msg2);
    g_free(msg2);
    
    msg=g_strdup("LiVES generator default sizes file version 2\n");
    buf=g_malloc(strlen(msg));
    bytes=read(fd,buf,strlen(msg));
    if (bytes==strlen(msg)&&!strncmp(buf,msg,strlen(msg))) {
      if (bytes==strlen(msg)) {
	read_generator_sizes(fd);
	d_print_done();
      }
    }
    else d_print_file_error_failed();
    
    close(fd);
    
    g_free(buf);
    g_free(msg);
  }

  return;
}


static void check_clear_all_button (void) {
  int modes=rte_getmodespk();
  int i,j;
  gboolean hasone=FALSE;

  for (i=0;i<prefs->rte_keys_virtual;i++) {
    for (j=modes-1;j>=0;j--) {
      if (rte_keymode_valid(i+1,j,TRUE)) hasone=TRUE;
    }
  }

  gtk_widget_set_sensitive (GTK_WIDGET(clear_all_button), hasone);
}



gboolean on_load_keymap_clicked (GtkButton *button, gpointer user_data) {
  int modes=rte_getmodespk();
  int i;
  int kfd;
  int version,nparams;
  int hlen;
  FILE *kfile=NULL;
  gchar *msg,*tmp;
  gint key,mode;
  gchar buff[65536];
  size_t linelen,bytes;
  gchar *whole=g_strdup (""),*whole2;
  GList *list=NULL,*new_list=NULL;
  gchar *hashname,*hashname_new=NULL;
  gboolean notfound=FALSE;
  gboolean has_error=FALSE;
  gboolean eof=FALSE;
  gint update=0;
  gchar *line=NULL;
  gchar *whashname;

  gchar *keymap_file=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap",NULL);
  gchar *keymap_file2=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap2",NULL);

  int *def_modes;

  def_modes=(int *)g_malloc(prefs->rte_keys_virtual*sizint);
  for (i=0;i<prefs->rte_keys_virtual;i++) def_modes[i]=-1;

  if ((kfd=open (keymap_file2,O_RDONLY))!=-1) {
    g_free(keymap_file);
    keymap_file=keymap_file2;
  }
  else {
    g_free(keymap_file2);
    keymap_file2=NULL;
  }

  msg=g_strdup_printf(_("Loading default keymap from %s..."),keymap_file);
  d_print(msg);
  g_free(msg);

  if (keymap_file2!=NULL) {
    if ((kfd=open(keymap_file,O_RDONLY))==-1) has_error=TRUE;
  }
  else {
    if (!(kfile=fopen (keymap_file,"r"))) {
      has_error=TRUE;
    }
  }

  if (has_error) {
    msg=g_strdup_printf (_("\n\nUnable to read from keymap file\n%s\nError code %d\n"),keymap_file,errno);
    g_free(keymap_file);
    do_error_dialog_with_check_transient (msg,FALSE,0,GTK_WINDOW(rte_window));
    g_free (msg);
    d_print_failed();
    g_free(def_modes);
    return FALSE;
  }

  on_clear_all_clicked(NULL,user_data);
  if (mainw->error) {
    mainw->error=FALSE;
    d_print_cancelled();
    g_free(def_modes);
    return FALSE;
  }

  if (keymap_file2==NULL) {
    while (fgets(buff,65536,kfile)) {
      if (buff!=NULL) {
	line=(g_strchomp (g_strchug(buff)));
	if ((linelen=strlen (line))) {
	  whole2=g_strconcat (whole,line,NULL);
	  if (whole2!=whole) g_free (whole);
	  whole=whole2;
	  if (linelen<(size_t)65535) {
	    list=g_list_append (list, g_strdup (whole));
	    g_free (whole);
	    whole=g_strdup ("");
	  }
	}
      }
    }
    fclose (kfile);

    if (!strcmp(g_list_nth_data(list,0),"LiVES keymap file version 2")||!strcmp(g_list_nth_data(list,0),"LiVES keymap file version 1")) update=1;
    if (!strcmp(g_list_nth_data(list,0),"LiVES keymap file version 3")) update=2;
  }
  else {
    // read version
    bytes=read(kfd,&version,sizint);
    if (bytes<sizint) {
      eof=TRUE;
    }
  }

  g_free (whole);

  for (i=1;(keymap_file2==NULL&&i<g_list_length(list))||(keymap_file2!=NULL&&!eof);i++) {
    gchar **array;

    if (keymap_file2==NULL) {
      line=(gchar *)g_list_nth_data(list,i);
    
      if (get_token_count(line,'|')<2) {
	d_print((tmp=g_strdup_printf(_("Invalid line %d in %s\n"),i,keymap_file)));
	g_free(tmp);
	continue;
      }
      
      array=g_strsplit (line,"|",-1);
      
      if (!strcmp(array[0],"defaults")) {
	g_strfreev(array);
	array=g_strsplit(line,"|",2);
	if (prefs->fxdefsfile!=NULL) g_free(prefs->fxdefsfile);
	prefs->fxdefsfile=g_strdup(array[1]);
	g_strfreev(array);
	continue;
      }
      
      if (!strcmp(array[0],"defaults")) {
	g_strfreev(array);
	array=g_strsplit(line,"|",2);
	if (prefs->fxdefsfile!=NULL) g_free(prefs->fxdefsfile);
	prefs->fxdefsfile=g_strdup(array[1]);
	g_strfreev(array);
	continue;
      }
      
      if (!strcmp(array[0],"sizes")) {
	g_strfreev(array);
	array=g_strsplit(line,"|",2);
	if (prefs->fxsizesfile!=NULL) g_free(prefs->fxsizesfile);
	prefs->fxsizesfile=g_strdup(array[1]);
	g_strfreev(array);
	continue;
      }
      
      key=atoi(array[0]);
    
      hashname=g_strdup(array[1]);
      g_strfreev(array);
      
      if (update>0) {
	if (update==1) hashname_new=g_strdup_printf("%d|Weed%s1\n",key,hashname);
	if (update==2) hashname_new=g_strdup_printf("%d|Weed%s\n",key,hashname);
	new_list=g_list_append(new_list,hashname_new);
	g_free(hashname);
	continue;
      }
    }
    else {
      //read key and hashname
      bytes=read(kfd,&key,sizint);
      if (bytes<sizint) {
	eof=TRUE;
	break;
      }

      bytes=read(kfd,&hlen,sizint);
      if (bytes<sizint) {
	eof=TRUE;
	break;
      }

      hashname=g_malloc(hlen+1);

      bytes=read(kfd,hashname,hlen);
      if (bytes<hlen) {
	eof=TRUE;
	g_free(hashname);
	break;
      }

      memset(hashname+hlen,0,1);

      array=g_strsplit(hashname,"|",-1);
      g_free(hashname);
      hashname=g_strdup(array[0]);
      g_strfreev(array);

    }

    if (key<1||key>prefs->rte_keys_virtual) {
      d_print((tmp=g_strdup_printf(_("Invalid key %d in %s\n"),key,keymap_file)));
      g_free(tmp);
      notfound=TRUE;
      g_free(hashname);
      continue;
    }

    def_modes[key-1]++;


    if (strncmp(hashname,"Weed",4)||strlen(hashname)<5) {
      d_print((tmp=g_strdup_printf(_("Invalid effect %s in %s\n"),hashname,keymap_file)));
      g_free(tmp);
      notfound=TRUE;
      g_free(hashname);
      continue;
    }

    // ignore "Weed"
    whashname=hashname+4;

    if ((mode=weed_add_effectkey(key,whashname,TRUE))==-1) {
      // could not locate effect
      d_print((tmp=g_strdup_printf(_("Unknown effect %s in %s\n"),whashname,keymap_file)));
      notfound=TRUE;
      g_free(hashname);
      continue;
    }

    g_free(hashname);

    if (mode==-2){
      d_print((tmp=g_strdup_printf(_("This version of LiVES cannot mix generators/non-generators on the same key (%d) !\n"),key)));
      g_free(tmp);
      continue;
    }
    if (mode==-3){
      d_print((tmp=g_strdup_printf(_("Too many effects bound to key %d.\n"),key)));
      g_free(tmp);
      continue;
    }
    if (rte_window!=NULL) {
      gtk_entry_set_text (GTK_ENTRY(combo_entries[(key-1)*modes+mode]),(tmp=rte_keymode_get_filter_name(key,mode)));
      g_free(tmp);
      type_label_set_text(key-1,mode);
    }


    if (keymap_file2!=NULL) {
      // read param defaults
      bytes=read(kfd,&nparams,sizint);
      if (bytes<sizint) {
	eof=TRUE;
	break;
      }
      if (nparams>0) {
	read_key_defaults(kfd,nparams,key-1,def_modes[key-1],version);
      }
    }
  }

  g_free(keymap_file);

  if (keymap_file2==NULL) {
    g_list_free_strings(list);
    g_list_free(list);

    if (update>0) {
      d_print(_("update required.\n"));
      on_save_keymap_clicked(NULL,new_list);
      g_list_free_strings(new_list);
      g_list_free(new_list);
      on_load_keymap_clicked(NULL,NULL);
    }
    else d_print_done();
  }
  else {
    close(kfd);
    d_print_done();
  }

  if (mainw->is_ready) {
    check_clear_all_button();
    if (notfound) do_warning_dialog_with_check_transient(_("\n\nSome effects could not be located.\n\n"),0,GTK_WINDOW(rte_window));
  }
  else load_rte_defs();
  g_free(def_modes);

  return FALSE;
}




void 
on_rte_info_clicked (GtkButton *button, gpointer user_data) {
  gint key_mode=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  gint key=(gint)(key_mode/modes);
  gint mode=key_mode-key*modes;

  gchar *type=rte_keymode_get_type(key+1,mode);
  weed_plant_t *filter;
  gchar *plugin_name;

  GtkWidget *rte_info_window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *textview;

  GtkTextBuffer *textbuffer;

  GtkWidget *hbuttonbox;
  GtkWidget *ok_button;

  gchar *filter_name;
  gchar *filter_author;
  gchar *filter_description;
  int filter_version;
  int weed_error;

  ////////////////////////

  if (!rte_keymode_valid(key+1,mode,TRUE)) return;

  plugin_name=rte_keymode_get_plugin_name(key+1,mode);
  filter=rte_keymode_get_filter(key+1,mode);
  filter_name=weed_get_string_value(filter,"name",&weed_error);
  filter_author=weed_get_string_value(filter,"author",&weed_error);
  if (weed_plant_has_leaf(filter,"description")) filter_description=weed_get_string_value(filter,"description",&weed_error);
  else filter_description=g_strdup(_("No Description"));

  filter_version=weed_get_int_value(filter,"version",&weed_error);

  rte_info_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (rte_info_window), g_strdup_printf(_("LiVES: Information for %s"),filter_name));
  gtk_widget_modify_bg(rte_info_window, GTK_STATE_NORMAL, &palette->normal_back);

  gtk_container_set_border_width (GTK_CONTAINER (rte_info_window), 40);
  gtk_window_set_transient_for(GTK_WINDOW(rte_info_window),GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
  gtk_window_set_position (GTK_WINDOW (rte_info_window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size (GTK_WINDOW (rte_info_window), RTE_INFO_WIDTH, RTE_INFO_HEIGHT);

  gtk_widget_show(rte_info_window);

  vbox = gtk_vbox_new (FALSE, 20);
  gtk_widget_show (vbox);
  gtk_container_add (GTK_CONTAINER (rte_info_window), vbox);

  label = gtk_label_new (g_strdup_printf(_("Effect name: %s"),filter_name));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
    
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = gtk_label_new (g_strdup_printf(_("Type: %s"),type));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = gtk_label_new (g_strdup_printf(_("Plugin name: %s"),plugin_name));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = gtk_label_new (g_strdup_printf(_("Author: %s"),filter_author));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = gtk_label_new (g_strdup_printf(_("Version: %d"),filter_version));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  hbox = gtk_hbox_new (FALSE, 10);
  gtk_widget_show (hbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  label = gtk_label_new (_("Description: "));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_widget_show (label);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  textview = gtk_text_view_new ();
  gtk_widget_show (textview);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_text(textview, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_base(textview, GTK_STATE_NORMAL, &palette->normal_back);
  }


  textbuffer=gtk_text_view_get_buffer (GTK_TEXT_VIEW (textview));
  
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
  
  gtk_text_buffer_set_text (textbuffer, filter_description, -1);
  gtk_box_pack_start (GTK_BOX (hbox), textview, TRUE, TRUE, 0);
  
  hbuttonbox = gtk_hbutton_box_new ();
  gtk_widget_show (hbuttonbox);
  gtk_box_pack_start (GTK_BOX (vbox), hbuttonbox, TRUE, TRUE, 0);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);

  ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (ok_button);

  gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);
  GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (ok_button);

  g_signal_connect (GTK_OBJECT (ok_button), "clicked",
		    G_CALLBACK (on_rtei_ok_clicked),
		    NULL);

  g_free(filter_name);
  g_free(filter_author);
  if (weed_plant_has_leaf(filter,"description")) g_free(filter_description);
  else weed_free(filter_description);
  g_free(plugin_name);
  g_free(type);
}





void on_clear_clicked (GtkButton *button, gpointer user_data) {
  // this is for the "delete" buttons, c.f. clear_all

  gint idx=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  gint key=(gint)(idx/modes);
  gint mode=idx-key*modes;

  int i,newmode;

  weed_delete_effectkey (key+1,mode);

  newmode=rte_key_getmode(key+1);
  g_signal_handler_block(mode_radios[key*modes+newmode],mode_ra_fns[key*modes+newmode]);
  rtew_set_mode_radio(key,newmode);
  g_signal_handler_unblock(mode_radios[key*modes+newmode],mode_ra_fns[key*modes+newmode]);
    
  for (i=mode;i<rte_getmodespk()-1;i++) {
    idx=key*modes+i;
    gtk_entry_set_text (GTK_ENTRY(combo_entries[idx]),gtk_entry_get_text(GTK_ENTRY(combo_entries[idx+1])));
    type_label_set_text(key,i);
  }
  idx++;
  gtk_entry_set_text (GTK_ENTRY(combo_entries[idx]),"");
  type_label_set_text(key,i);

  if (!rte_keymode_valid(key+1,0,TRUE)) rtew_set_keych(key,FALSE);

  check_clear_all_button();

}


void on_params_clicked (GtkButton *button, gpointer user_data) {
  gint idx=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  gint key=(gint)(idx/modes);
  gint mode=idx-key*modes;

  weed_plant_t *inst;
  lives_rfx_t *rfx;

  if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) {
    weed_plant_t *filter=rte_keymode_get_filter(key+1,mode);
    if (filter==NULL) return;
    inst=weed_instance_from_filter(filter);
    apply_key_defaults(inst,key,mode);
  }
  else weed_instance_ref(inst);

  if (fx_dialog[1]!=NULL) {
    rfx=g_object_get_data (G_OBJECT (fx_dialog[1]),"rfx");
    gtk_widget_destroy(fx_dialog[1]);
    on_paramwindow_cancel_clicked2(NULL,rfx);
  }

  rfx=weed_to_rfx(inst,FALSE);
  rfx->min_frames=-1;
  keyw=key;
  modew=mode;
  on_render_fx_pre_activate(NULL,rfx);

  // record the key so we know whose parameters to record later
  weed_set_int_value(rfx->source,"host_hotkey",key);

  g_object_set_data (G_OBJECT (fx_dialog[1]),"key",GINT_TO_POINTER (key));
  g_object_set_data (G_OBJECT (fx_dialog[1]),"mode",GINT_TO_POINTER (mode));
  g_object_set_data (G_OBJECT (fx_dialog[1]),"rfx",rfx);
}


gboolean
on_rtew_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {

  if (hash_list!=NULL) {
    g_list_free_strings(hash_list);
    g_list_free(hash_list);
  }

  if (name_list!=NULL) {
    g_list_free_strings(name_list);
    g_list_free(name_list);
  }

  if (name_type_list!=NULL) {
    g_list_free_strings(name_type_list);
    g_list_free(name_type_list);
  }

  g_free(key_checks);
  g_free(key_grabs);
  g_free(mode_radios);
  g_free(combo_entries);
  g_free(ch_fns);
  g_free(mode_ra_fns);
  g_free(gr_fns);
  g_free(nlabels);
  g_free(type_labels);
  g_free(info_buttons);
  g_free(param_buttons);
  g_free(clear_buttons);
  rte_window=NULL;
  return FALSE;
}


void 
on_rtew_ok_clicked (GtkButton *button, gpointer user_data) {
  gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
  on_rtew_delete_event (NULL,NULL,NULL);
}




void do_mix_error(void) {
  do_error_dialog_with_check_transient(_("\n\nThis version of LiVES does not allowing mixing of generators and non-generators on the same key.\n\n"),FALSE,0,GTK_WINDOW(rte_window));
  return;
}





void fx_changed (GtkItem *item, gpointer user_data) {
  gint key_mode=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  gint key=(gint)(key_mode/modes);
  gint mode=key_mode-key*modes;
  gchar *hashname1=g_strdup(g_object_get_data(G_OBJECT(item),"hashname"));
  gchar *hashname2=g_strdup(g_object_get_data(G_OBJECT(combo_entries[key_mode]),"hashname"));
  gint error;
  gchar *tmp;

  int i;

  if (!strcmp(hashname1,hashname2)) {
    g_free(hashname1);
    g_free(hashname2);
    return;
  }

  if (!rte_keymode_valid(key+1,mode,TRUE)) {
    for (i=mode-1;i>=0;i--) {
      if (rte_keymode_valid(key+1,i,TRUE)) {
	mode=i+1;
	i=-1;
      }
      if (i==0) mode=0;
    }
  }

  gtk_widget_grab_focus (combo_entries[key_mode]);

  if ((error=rte_switch_keymode (key+1, mode, hashname1))<0) {
    gtk_entry_set_text (GTK_ENTRY (combo_entries[key_mode]),(tmp=rte_keymode_get_filter_name(key+1,mode)));
    g_free(tmp);

    // this gets called twice, unfortunately...may be a bug in gtk+
    if (error==-2) do_mix_error();
    if (error==-1) {
      d_print((tmp=g_strdup_printf(_("LiVES could not locate the effect %s.\n"),rte_keymode_get_filter_name(key+1,mode))));
      g_free(tmp);
    }
    g_free(hashname1);
    g_free(hashname2);
    return;
  }

  g_object_set_data(G_OBJECT(combo_entries[key_mode]),"hashname",g_strdup(hashname1));

  g_free(hashname2);
  g_free(hashname1);
    
  type_label_set_text(key,mode);

  check_clear_all_button();

}




GtkWidget * create_rte_window (void) {
  int i,j,idx,fx_idx,error;

  gint winsize_h;
  gint winsize_v;

  gint scr_width,scr_height;

  GtkWidget *rte_window;
  GtkWidget *table;
  GtkWidget *hbox;
  GtkWidget *hbox2;
  GtkWidget *eventbox;
  GSList *mode_group = NULL;
  GSList *grab_group = NULL;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *ok_button;
  GtkWidget *top_vbox;
  GtkWidget *hbuttonbox;

  GtkWidget *scrolledwindow;

  GtkAccelGroup *rtew_accel_group;

  int modes=rte_getmodespk();

  GList *list;
  GtkWidget *item;

  gchar *tmp;

  ///////////////////////////////////////////////////////////////////////////


  if (prefs->gui_monitor==0) {
    scr_width=mainw->scr_width;
    scr_height=mainw->scr_height;
  }
  else {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }

  winsize_h=scr_width-100;
  winsize_v=scr_height-200;

  key_checks=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*sizeof(GtkWidget *));
  key_grabs=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*sizeof(GtkWidget *));
  mode_radios=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  combo_entries=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  info_buttons=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  param_buttons=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  clear_buttons=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  nlabels=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  type_labels=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));

  ch_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*sizeof(gulong));
  gr_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*sizeof(gulong));
  mode_ra_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(gulong));

  rte_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_modify_bg(rte_window, GTK_STATE_NORMAL, &palette->menu_and_bars);
  gtk_window_set_title (GTK_WINDOW (rte_window), _("LiVES: Real time effect mapping"));
  gtk_window_add_accel_group (GTK_WINDOW (rte_window), mainw->accel_group);

  table = gtk_table_new (prefs->rte_keys_virtual, modes+1, FALSE);

  gtk_table_set_row_spacings (GTK_TABLE (table), 16);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);

  // dummy button for "no grab", we dont show this...there is a button instead
  dummy_radio = gtk_radio_button_new_with_label (grab_group, _("Key grab"));
  grab_group = gtk_radio_button_group (GTK_RADIO_BUTTON (dummy_radio));

  name_list=weed_get_all_names(1);
  name_type_list=weed_get_all_names(2);
  hash_list=weed_get_all_names(3);

  for (i=0;i<prefs->rte_keys_virtual;i++) {

    hbox = gtk_hbox_new (FALSE, 10);
    gtk_table_attach (GTK_TABLE (table), hbox, i, i+1, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
    
    label = gtk_label_new (g_strdup_printf(_("Ctrl-%d"),i+1));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

    key_checks[i] = gtk_check_button_new ();
    eventbox=gtk_event_box_new();
    label=gtk_label_new (_("Key active"));

    gtk_container_add(GTK_CONTAINER(eventbox),label);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      key_checks[i]);

    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    
    hbox2 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox2), key_checks[i], FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);
    GTK_WIDGET_SET_FLAGS (key_checks[i], GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_checks[i]),mainw->rte&(GU641<<i));

    ch_fns[i]=g_signal_connect_after (GTK_OBJECT (key_checks[i]), "toggled",
                      G_CALLBACK (rte_on_off_callback_hook),GINT_TO_POINTER (i+1));



    hbox2 = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);

    key_grabs[i]=gtk_radio_button_new(grab_group);
    grab_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (key_grabs[i]));

    gtk_box_pack_start (GTK_BOX (hbox2), key_grabs[i], FALSE, FALSE, 10);

    label=gtk_label_new_with_mnemonic (_ ("Key grab"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),key_grabs[i]);

    eventbox=gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox),label);
    gtk_tooltips_set_tip (mainw->tooltips, eventbox, _("Grab keyboard for this effect key"), NULL);
    gtk_tooltips_copy(key_grabs[i],eventbox);
    g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		      G_CALLBACK (label_act_toggle),
		      key_grabs[i]);
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
      gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
    }
    gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_grabs[i]),mainw->rte_keys==i);

    gr_fns[i]=g_signal_connect_after (GTK_OBJECT (key_grabs[i]), "toggled",
				      G_CALLBACK (grabkeys_callback_hook),GINT_TO_POINTER (i));

    mode_group=NULL;

    clear_all_button = gtk_button_new_with_mnemonic (_("_Clear all effects"));

    for (j=0;j<modes;j++) {
      idx=i*modes+j;
      hbox = gtk_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (table), hbox, i, i+1, j+1, j+2,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);


      hbox2 = gtk_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);
      
      mode_radios[idx]=gtk_radio_button_new(mode_group);
      mode_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (mode_radios[idx]));
      
      gtk_box_pack_start (GTK_BOX (hbox2), mode_radios[idx], FALSE, FALSE, 10);

      label=gtk_label_new_with_mnemonic (_ ("Mode active"));
      gtk_label_set_mnemonic_widget (GTK_LABEL (label),mode_radios[idx]);

      eventbox=gtk_event_box_new();
      gtk_container_add(GTK_CONTAINER(eventbox),label);
      g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
			G_CALLBACK (label_act_toggle),
			mode_radios[idx]);
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
	gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
      }
      gtk_box_pack_start (GTK_BOX (hbox2), eventbox, FALSE, FALSE, 10);


      if (rte_key_getmode(i+1)==j) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mode_radios[idx]),TRUE);

      mode_ra_fns[idx]=g_signal_connect_after (GTK_OBJECT (mode_radios[idx]), "toggled",
					       G_CALLBACK (rtemode_callback_hook),GINT_TO_POINTER (idx));

      type_labels[idx] = gtk_label_new ("");
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(type_labels[idx], GTK_STATE_NORMAL, &palette->normal_fore);
      }

      info_buttons[idx] = gtk_button_new_with_label (_("Info"));
      param_buttons[idx] = gtk_button_new_with_label (_("Set Parameters"));
      clear_buttons[idx] = gtk_button_new_with_label (_("Clear"));

      vbox = gtk_vbox_new (FALSE, 15);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 13);

      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      nlabels[idx] = gtk_label_new (_("Effect name:"));
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(nlabels[idx], GTK_STATE_NORMAL, &palette->normal_fore);
      }

      gtk_box_pack_start (GTK_BOX (hbox), nlabels[idx], FALSE, FALSE, 0);

      combo = gtk_combo_new ();
      gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);

      combo_entries[idx] = GTK_COMBO (combo)->entry;
      g_object_set_data (G_OBJECT(combo_entries[idx]),"hashname","");

      // fill names of our effects
      fx_idx=0;
      list=name_type_list;
      
      while (list!=NULL) {
	weed_plant_t *filter=get_weed_filter(weed_get_idx_for_hashname(g_list_nth_data(hash_list,fx_idx),FALSE));
	int filter_flags=weed_get_int_value(filter,"flags",&error);
	if ((enabled_in_channels(filter,FALSE)>1&&!has_video_chans_in(filter,FALSE))||(weed_plant_has_leaf(filter,"host_menu_hide")&&weed_get_boolean_value(filter,"host_menu_hide",&error)==WEED_TRUE)||(filter_flags&WEED_FILTER_IS_CONVERTER)) {
	  list = list->next;
	  fx_idx++;
	  continue; // skip audio transitions and hidden entries
	}
	item = gtk_list_item_new_with_label ((gchar *) list->data);
	gtk_widget_show(item);
	gtk_container_add (GTK_CONTAINER (GTK_COMBO(combo)->list), item);
	g_object_set_data (G_OBJECT(item),"hashname",g_list_nth_data(hash_list,fx_idx));
	gtk_combo_set_item_string(GTK_COMBO(combo),GTK_ITEM(item),g_list_nth_data(name_list,fx_idx));

	if (fx_idx==rte_keymode_get_filter_idx(i+1,j)) {
	  g_object_set_data (G_OBJECT(combo_entries[idx]),"hashname",g_list_nth_data(hash_list,fx_idx));
	}

	g_signal_connect (G_OBJECT(item), "select", G_CALLBACK (fx_changed), GINT_TO_POINTER(idx));
	list = list->next;
	fx_idx++;
      }
      
      gtk_entry_set_text (GTK_ENTRY (combo_entries[idx]),(tmp=rte_keymode_get_filter_name(i+1,j)));
      g_free(tmp);
      gtk_entry_set_editable (GTK_ENTRY (combo_entries[idx]), FALSE);
      
      hbox = gtk_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      g_signal_connect (GTK_OBJECT (info_buttons[idx]), "clicked",
			G_CALLBACK (on_rte_info_clicked),GINT_TO_POINTER (idx));

      g_signal_connect (GTK_OBJECT (clear_buttons[idx]), "clicked",
			G_CALLBACK (on_clear_clicked),GINT_TO_POINTER (idx));

      g_signal_connect (GTK_OBJECT (param_buttons[idx]), "clicked",
			G_CALLBACK (on_params_clicked),GINT_TO_POINTER (idx));
      
      type_label_set_text(i,j);

      gtk_box_pack_start (GTK_BOX (hbox), type_labels[idx], FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), info_buttons[idx], FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), param_buttons[idx], FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), clear_buttons[idx], FALSE, FALSE, 0);

      }
  }


  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), table);

  gtk_widget_set_size_request (scrolledwindow, winsize_h, winsize_v);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(gtk_bin_get_child (GTK_BIN (scrolledwindow)), GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);

  top_vbox = gtk_vbox_new (FALSE, 15);

  gtk_box_pack_start (GTK_BOX (top_vbox), dummy_radio, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (rte_window), top_vbox);

  hbuttonbox = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (top_vbox), hbuttonbox, FALSE, TRUE, 20);
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);

  gtk_container_add (GTK_CONTAINER (hbuttonbox), clear_all_button);
  GTK_WIDGET_SET_FLAGS (clear_all_button, GTK_CAN_DEFAULT);

  save_keymap_button = gtk_button_new_with_mnemonic (_("_Save as default keymap"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), save_keymap_button);
  GTK_WIDGET_SET_FLAGS (save_keymap_button, GTK_CAN_DEFAULT);

  load_keymap_button = gtk_button_new_with_mnemonic (_("_Load default keymap"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), load_keymap_button);
  GTK_WIDGET_SET_FLAGS (load_keymap_button, GTK_CAN_DEFAULT);

  ok_button = gtk_button_new_with_mnemonic (_("Close _window"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);
  GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT);
  

  rtew_accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (rte_window), rtew_accel_group);

  gtk_widget_add_accelerator (ok_button, "activate", rtew_accel_group,
                              GDK_Escape, 0, 0);
  g_signal_connect (GTK_OBJECT (rte_window), "delete_event",
		    G_CALLBACK (on_rtew_ok_clicked),
		    NULL);

  g_signal_connect (GTK_OBJECT (ok_button), "clicked",
		    G_CALLBACK (on_rtew_ok_clicked),
		    NULL);

  g_signal_connect (GTK_OBJECT (save_keymap_button), "clicked",
		    G_CALLBACK (on_save_keymap_clicked),
		    NULL);

  g_signal_connect (GTK_OBJECT (load_keymap_button), "clicked",
		    G_CALLBACK (on_load_keymap_clicked),
		    GINT_TO_POINTER(1));

  g_signal_connect (GTK_OBJECT (clear_all_button), "clicked",
		    G_CALLBACK (on_clear_all_clicked),
		    GINT_TO_POINTER(1));

  gtk_widget_show_all(rte_window);
  gtk_widget_hide(dummy_radio);

  if (prefs->gui_monitor!=0) {
    gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-rte_window->allocation.width)/2;
    gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-rte_window->allocation.height)/2;
    gtk_window_set_screen(GTK_WINDOW(rte_window),mainw->mgeom[prefs->gui_monitor-1].screen);
    gtk_window_move(GTK_WINDOW(rte_window),xcen,ycen);
  }

  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(rte_window));
  }
  return rte_window;
}

void refresh_rte_window (void) {
  if (rte_window!=NULL) {
    on_rtew_delete_event(NULL,NULL,NULL);
    rte_window=create_rte_window();
    gtk_widget_show (rte_window);
  }
}


void 
on_assign_rte_keys_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (rte_window!=NULL) {
    on_rtew_ok_clicked(GTK_BUTTON(dummy_radio), user_data);
    return;
  }
  
  rte_window=create_rte_window();
  gtk_widget_show (rte_window);
}


void rtew_set_keych (gint key, gboolean on) {
  g_signal_handler_block(key_checks[key],ch_fns[key]);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(key_checks[key]),on);
  g_signal_handler_unblock(key_checks[key],ch_fns[key]);
}


void rtew_set_keygr (gint key) {
  if (key>=0) {
    g_signal_handler_block(key_grabs[key],gr_fns[key]);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(key_grabs[key]),TRUE);
    g_signal_handler_unblock(key_grabs[key],gr_fns[key]);
  }
  else {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dummy_radio),TRUE);
  }
}

void rtew_set_mode_radio (gint key, gint mode) {
  int modes=rte_getmodespk();
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mode_radios[key*modes+mode]),TRUE);
}



void redraw_pwindow (gint key, gint mode) {
  gint keyw=0,modew=0;
  GList *child_list;
  int i;
  lives_rfx_t *rfx;

  if (fx_dialog[1]!=NULL) {
    rfx=g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
    if (!rfx->is_template) {
      keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
      modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    }
    if (rfx->is_template||(key==keyw&&mode==modew)) {
      // rip out the contents
      if (mainw->invis==NULL) mainw->invis=gtk_vbox_new(FALSE,0);
      child_list=gtk_container_get_children(GTK_CONTAINER(GTK_DIALOG(fx_dialog[1])->vbox));
      for (i=0;i<g_list_length(child_list);i++) {
	GtkWidget *widget=g_list_nth_data(child_list,i);
	if (widget!=GTK_DIALOG (fx_dialog[1])->action_area) {
	  // we have to do this, because using gtk_widget_destroy() here 
	  // can causes a crash [bug in gtk+ ???]
	  gtk_widget_reparent (widget,mainw->invis);
	}
      }
      if (child_list!=NULL) g_list_free(child_list);
      on_paramwindow_cancel_clicked(NULL,NULL);
      restore_pwindow(rfx);
    }
  }
}



void restore_pwindow (lives_rfx_t *rfx) {
  if (fx_dialog[1]!=NULL) {
    make_param_box(GTK_VBOX (GTK_DIALOG(fx_dialog[1])->vbox), rfx);
    gtk_widget_show_all (GTK_DIALOG(fx_dialog[1])->vbox);
    gtk_widget_queue_draw(fx_dialog[1]);
  }
}


void update_pwindow (gint key, gint i, GList *list) {
  const weed_plant_t *inst;
  lives_rfx_t *rfx;
  gint keyw,modew;

  if (fx_dialog[1]!=NULL) {
    keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
    modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    if (key==keyw) {
      if ((inst=rte_keymode_get_instance(key+1,modew))==NULL) return;
      rfx=g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
      set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
    }
  }
}

void rte_set_defs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gint idx=GPOINTER_TO_INT(user_data);
  weed_plant_t *filter=get_weed_filter(idx);
  lives_rfx_t *rfx;

  if (fx_dialog[1]!=NULL) {
    rfx=g_object_get_data (G_OBJECT (fx_dialog[1]),"rfx");
    gtk_widget_destroy(fx_dialog[1]);
    on_paramwindow_cancel_clicked2(NULL,rfx);
  }

  rfx=weed_to_rfx(filter,TRUE);
  rfx->min_frames=-1;
  on_render_fx_pre_activate(NULL,rfx);

}



void rte_set_key_defs (GtkButton *button, lives_rfx_t *rfx) {
  gint key,mode;
  if (mainw->textwidget_focus!=NULL) after_param_text_changed(mainw->textwidget_focus,rfx);

  if (rfx->num_params>0) {
    key=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
    mode=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    set_key_defaults(rfx->source,key,mode);
  }
}




void rte_set_defs_ok (GtkButton *button, lives_rfx_t *rfx) {
  int i;
  weed_plant_t **ptmpls,*filter,*copy_param=NULL;
  int error;
  lives_colRGB24_t *rgbp;

  if (mainw->textwidget_focus!=NULL) after_param_text_changed(mainw->textwidget_focus,rfx);

  if (rfx->num_params>0) {
    filter=weed_get_plantptr_value(rfx->source,"filter_class",&error);
    ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
    for (i=0;i<rfx->num_params;i++) {
      switch (rfx->params[i].type) {
      case LIVES_PARAM_COLRGB24:
	rgbp=(lives_colRGB24_t *)rfx->params[i].value;
	if (rfx->params[i].copy_to!=-1) copy_param=weed_inst_in_param(rfx->source,rfx->params[i].copy_to,FALSE);
	update_weed_color_value(ptmpls[i],i,copy_param,rgbp->red,rgbp->green,rgbp->blue,0);
	break;
      case LIVES_PARAM_STRING:
	weed_set_string_value(ptmpls[i],"host_default",rfx->params[i].value);
	break;
      case LIVES_PARAM_STRING_LIST:
	weed_set_int_array(ptmpls[i],"host_default",1,rfx->params[i].value);
	break;
      case LIVES_PARAM_NUM:
	if (rfx->params[i].dp>0) weed_set_double_array(ptmpls[i],"host_default",1,rfx->params[i].value);
	else weed_set_int_array(ptmpls[i],"host_default",1,rfx->params[i].value);
	break;
      case LIVES_PARAM_BOOL:
	weed_set_boolean_array(ptmpls[i],"host_default",1,rfx->params[i].value);
	break;
      default:
	break;
      }
    }
    weed_free(ptmpls);
  }

  on_paramwindow_cancel_clicked(button,rfx);
  fx_dialog[1]=NULL;
}



void rte_set_defs_cancel (GtkButton *button, lives_rfx_t *rfx) {
  on_paramwindow_cancel_clicked(button,rfx);
  fx_dialog[1]=NULL;
}


void load_default_keymap(void) {
  // called on startup
  gchar *keymap_file=g_strdup_printf("%s/%sdefault.keymap",capable->home_dir,LIVES_CONFIG_DIR);
  gchar *keymap_template=g_strdup_printf("%s%sdefault.keymap",prefs->prefix_dir,DATA_DIR);
  gchar *com,*tmp;

  pthread_mutex_lock(&mainw->gtk_mutex);
  if (!g_file_test (keymap_file, G_FILE_TEST_EXISTS)) {
    com=g_strdup_printf("/bin/cp %s %s",keymap_template,keymap_file);
    dummyvar=system(com);
    g_free(com);
  }
  if (!g_file_test (keymap_file, G_FILE_TEST_EXISTS)) {
    // give up
    d_print((tmp=g_strdup_printf(_("Unable to create default keymap file: %s\nPlease make sure your home directory is writable.\n"),keymap_file)));
    g_free(tmp);
    g_free(keymap_file);
    g_free(keymap_template);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    return;
  }
  on_load_keymap_clicked(NULL,NULL);
  g_free(keymap_file);
  g_free(keymap_template);
  pthread_mutex_unlock(&mainw->gtk_mutex);
}
