// rte_window.c
// LiVES (lives-exe)
// (c) G. Finch 2005 - 2013
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include <errno.h>
#include <sys/stat.h>

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-effects.h>
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

static GtkWidget **key_checks;
static GtkWidget **key_grabs;
static GtkWidget **mode_radios;
static GtkWidget **combo_entries;
static GtkWidget *dummy_radio;
static GtkWidget **nlabels;
static GtkWidget **type_labels;
static GtkWidget **param_buttons;
static GtkWidget **conx_buttons;
static GtkWidget **clear_buttons;
static GtkWidget **info_buttons;
static GtkWidget *clear_all_button;
static GtkWidget *save_keymap_button;
static GtkWidget *load_keymap_button;

static GtkWidget *datacon_dialog=NULL;

static gulong *ch_fns;
static gulong *gr_fns;
static gulong *mode_ra_fns;

static int keyw=-1,modew=-1;

static GList *hash_list;
static GList *name_list;
static GList *name_type_list;


static boolean ca_canc;

//////////////////////////////////////////////////////////////////////////////


void type_label_set_text (int key, int mode) {
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



boolean on_clear_all_clicked (GtkButton *button, gpointer user_data) {
  int modes=rte_getmodespk();
  int i,j;

  ca_canc=FALSE;

  // prompt for "are you sure ?"
  if (user_data!=NULL) if (!do_warning_dialog_with_check_transient
			   ((_("\n\nUnbind all effects from all keys/modes.\n\nAre you sure ?\n\n")),
			    0,GTK_WINDOW(rte_window))) {
      ca_canc=TRUE;
      return FALSE;
    }

  pconx_delete_all();
  cconx_delete_all();

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


static boolean save_keymap2_file(gchar *kfname) {
  // save perkey defaults
  int slen;
  int version=1;
  int modes=rte_getmodespk();
  int kfd;
  int retval;
  int i,j;

  gchar *hashname,*tmp;

  do {
    retval=0;
    kfd=creat(kfname,S_IRUSR|S_IWUSR);
    if (kfd==-1) {
      retval=do_write_failed_error_s_with_retry (kfname,g_strerror(errno),GTK_WINDOW(rte_window));
    }
    else {
      mainw->write_failed=FALSE;
      
      lives_write_le(kfd,&version,4,TRUE);
      
      for (i=1;i<=prefs->rte_keys_virtual;i++) {
	if (mainw->write_failed) break;
	for (j=0;j<modes;j++) {
	  if (rte_keymode_valid(i,j,TRUE)) {
	    lives_write_le(kfd,&i,4,TRUE);
	    if (mainw->write_failed) break;
	    hashname=g_strdup_printf("Weed%s",(tmp=make_weed_hashname(rte_keymode_get_filter_idx(i,j),TRUE,FALSE)));
	    g_free(tmp);
	    slen=strlen(hashname);
	    lives_write_le(kfd,&slen,4,TRUE);
	    lives_write(kfd,hashname,slen,TRUE);
	    g_free(hashname);
	    write_key_defaults(kfd,i-1,j);
	  }
	}
      }
      close(kfd);
      
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(kfname,NULL,GTK_WINDOW(rte_window));
	mainw->write_failed=FALSE;
      }
    }
  } while (retval==LIVES_RETRY);

  if (retval==LIVES_CANCEL) return FALSE;
  return TRUE;

}



static boolean save_keymap3_file(gchar *kfname) {
  // save data connections

  gchar *hashname;

  int version=1;

  int slen;
  int kfd;
  int retval;
  int count=0,totcons=0,nconns;

  register int i,j=0;

  do {
    retval=0;
    kfd=creat(kfname,S_IRUSR|S_IWUSR);
    if (kfd==-1) {
      retval=do_write_failed_error_s_with_retry (kfname,g_strerror(errno),GTK_WINDOW(rte_window));
    }
    else {
      mainw->write_failed=FALSE;
      
      lives_write_le(kfd,&version,4,TRUE);


      if (mainw->cconx!=NULL) {
	lives_cconnect_t *cconx=mainw->cconx;
	int nchans;

	while (cconx!=NULL) {
	  count++;
	  cconx=cconx->next;
	}

	lives_write_le(kfd,&count,4,TRUE);
	if (mainw->write_failed) goto write_failed1;

	cconx=mainw->cconx;
	while (cconx!=NULL) {
	  lives_write_le(kfd,&cconx->okey,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  lives_write_le(kfd,&cconx->omode,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  hashname=make_weed_hashname(rte_keymode_get_filter_idx(cconx->okey+1,cconx->omode),TRUE,FALSE);
	  slen=strlen(hashname);
	  lives_write_le(kfd,&slen,4,TRUE);
	  lives_write(kfd,hashname,slen,TRUE);
	  g_free(hashname);

	  nchans=cconx->nchans;
	  lives_write_le(kfd,&nchans,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  for (i=0;i<nchans;i++) {
	    lives_write_le(kfd,&cconx->chans[i],4,TRUE);
	    if (mainw->write_failed) goto write_failed1;

	    nconns=cconx->nconns[i];
	    lives_write_le(kfd,&nconns,4,TRUE);
	    if (mainw->write_failed) goto write_failed1;

	    totcons+=nconns;

	    while (j<totcons) {
	      lives_write_le(kfd,&cconx->ikey[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      lives_write_le(kfd,&cconx->imode[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;
	      
	      hashname=make_weed_hashname(rte_keymode_get_filter_idx(cconx->ikey[j]+1,cconx->imode[j]),TRUE,FALSE);
	      slen=strlen(hashname);
	      lives_write_le(kfd,&slen,4,TRUE);
	      lives_write(kfd,hashname,slen,TRUE);
	      g_free(hashname);

	      lives_write_le(kfd,&cconx->icnum[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      j++;
	    }

	  }

	  cconx=cconx->next;
	}
      }
      else {
	lives_write_le(kfd,&count,4,TRUE);
	if (mainw->write_failed) goto write_failed1;
      }


      if (mainw->pconx!=NULL) {
	lives_pconnect_t *pconx=mainw->pconx;

	int nparams;

	totcons=0;
	count=0;
	j=0;

	while (pconx!=NULL) {
	  count++;
	  pconx=pconx->next;
	}

	lives_write_le(kfd,&count,4,TRUE);
	if (mainw->write_failed) goto write_failed1;

	pconx=mainw->pconx;
	while (pconx!=NULL) {
	  lives_write_le(kfd,&pconx->okey,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  lives_write_le(kfd,&pconx->omode,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  hashname=make_weed_hashname(rte_keymode_get_filter_idx(pconx->okey+1,pconx->omode),TRUE,FALSE);
	  slen=strlen(hashname);
	  lives_write_le(kfd,&slen,4,TRUE);
	  lives_write(kfd,hashname,slen,TRUE);
	  g_free(hashname);

	  nparams=pconx->nparams;
	  lives_write_le(kfd,&nparams,4,TRUE);
	  if (mainw->write_failed) goto write_failed1;

	  for (i=0;i<nparams;i++) {
	    lives_write_le(kfd,&pconx->params[i],4,TRUE);
	    if (mainw->write_failed) goto write_failed1;

	    nconns=pconx->nconns[i];
	    lives_write_le(kfd,&nconns,4,TRUE);
	    if (mainw->write_failed) goto write_failed1;

	    totcons+=nconns;

	    while (j<totcons) {
	      lives_write_le(kfd,&pconx->ikey[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      lives_write_le(kfd,&pconx->imode[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      hashname=make_weed_hashname(rte_keymode_get_filter_idx(pconx->ikey[j]+1,pconx->imode[j]),TRUE,FALSE);
	      slen=strlen(hashname);
	      lives_write_le(kfd,&slen,4,TRUE);
	      lives_write(kfd,hashname,slen,TRUE);
	      g_free(hashname);

	      lives_write_le(kfd,&pconx->ipnum[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      lives_write_le(kfd,&pconx->autoscale[j],4,TRUE);
	      if (mainw->write_failed) goto write_failed1;

	      j++;
	    }

	  }

	  pconx=pconx->next;
	}

      }
      else {
	lives_write_le(kfd,&count,4,TRUE);
	if (mainw->write_failed) goto write_failed1;
      }


    write_failed1:
      close(kfd);
      
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(kfname,NULL,GTK_WINDOW(rte_window));
	mainw->write_failed=FALSE;
      }
    }
  } while (retval==LIVES_RETRY);

  if (retval==LIVES_CANCEL) return FALSE;
  return TRUE;

}



static boolean on_save_keymap_clicked (GtkButton *button, gpointer user_data) {
  // save as keymap type 1 file - to allow backwards compatibility with older versions of LiVES
  // default.keymap

  // format is text
  // key|hashname



  // if we have per key defaults, then we add an extra file:
  // default.keymap2

  // format is binary
  // (4 bytes key) (4 bytes hlen) (hlen bytes hashname) then a dump of the weed plant default values

  // if we have data connections, we will save a third file


  FILE *kfile;
  GList *list=NULL;

  gchar *msg,*tmp;
  gchar *keymap_file=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap",NULL);
  gchar *keymap_file2=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap2",NULL);
  gchar *keymap_file3=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap3",NULL);

  boolean update=FALSE;

  int modes=rte_getmodespk();
  int i,j;
  int retval;
 
  if (button!=NULL) {
    if (!do_warning_dialog_with_check_transient
	((_("\n\nClick 'OK' to save this keymap as your default\n\n")),0,GTK_WINDOW(rte_window))) {
      g_free(keymap_file3);
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

  do {
    retval=0;
    if (!(kfile=fopen(keymap_file,"w"))) {
      msg=g_strdup_printf (_("\n\nUnable to write keymap file\n%s\nError was %s\n"),keymap_file,g_strerror(errno));
      retval=do_abort_cancel_retry_dialog (msg,GTK_WINDOW(rte_window));
      g_free (msg);
    }
    else {
      mainw->write_failed=FALSE;
      lives_fputs("LiVES keymap file version 4\n",kfile);

      if (!update) {
	for (i=1;i<=prefs->rte_keys_virtual;i++) {
	  for (j=0;j<modes;j++) {
	    if (rte_keymode_valid(i,j,TRUE)) {
	      lives_fputs(g_strdup_printf("%d|Weed%s\n",i,(tmp=make_weed_hashname(rte_keymode_get_filter_idx(i,j),TRUE,FALSE))),kfile);
	      g_free(tmp);
	    }
	  }
	}
      }
      else {
	for (i=0;i<g_list_length(list);i++) {
	  lives_fputs((gchar *)g_list_nth_data(list,i),kfile);
	}
      }
      
      fclose (kfile);
    }

    if (mainw->write_failed) {
      retval=do_write_failed_error_s_with_retry(keymap_file,NULL,GTK_WINDOW(rte_window));
    }
  } while (retval==LIVES_RETRY);


  // if we have default values, save them
  if (has_key_defaults()) {
    if (!save_keymap2_file(keymap_file2)) {
      unlink(keymap_file2);
      retval=LIVES_CANCEL;
    }
  }
  else unlink(keymap_file2);


  // if we have data connections, save them
  if (mainw->pconx!=NULL||mainw->cconx!=NULL) {
    if (!save_keymap3_file(keymap_file3)) {
      unlink(keymap_file3);
      retval=LIVES_CANCEL;
    }
  }
  else unlink(keymap_file3);


  g_free(keymap_file3);
  g_free(keymap_file2);
  g_free(keymap_file);

  if (retval==LIVES_CANCEL) d_print_file_error_failed();
  else d_print_done();

  return FALSE;
}




void on_save_rte_defs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  int fd,i;
  int retval;
  int numfx;
  gchar *msg;

  if (prefs->fxdefsfile==NULL) {
    prefs->fxdefsfile=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"fxdefs",NULL);
  }

  if (prefs->fxsizesfile==NULL) {
    prefs->fxsizesfile=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"fxsizes",NULL);
  }

  msg=g_strdup_printf(_("Saving real time effect defaults to %s..."),prefs->fxdefsfile);
  d_print(msg);
  g_free(msg);

  numfx=rte_get_numfilters(FALSE);

  do {
    retval=0;
    if ((fd=open(prefs->fxdefsfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
      msg=g_strdup_printf (_("\n\nUnable to write defaults file\n%s\nError code %d\n"),prefs->fxdefsfile,errno);
      retval=do_abort_cancel_retry_dialog (msg,GTK_WINDOW(rte_window));
      g_free (msg);
    }
    else {
#ifdef IS_MINGW
      setmode(fd, O_BINARY);
#endif
      msg=g_strdup("LiVES filter defaults file version 1.1\n");
      mainw->write_failed=FALSE;
      lives_write(fd,msg,strlen(msg),TRUE);
      g_free(msg);

      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(prefs->fxdefsfile,NULL,GTK_WINDOW(rte_window));
      }
      else {
	// break on file write error
	for (i=0;i<numfx;i++) {
	  if (!write_filter_defaults(fd,i)) {
	    retval=do_write_failed_error_s_with_retry(prefs->fxdefsfile,NULL,GTK_WINDOW(rte_window));
	    break;
	  }
	}
      }
      close (fd);
    }
  } while (retval==LIVES_RETRY);


  if (retval==LIVES_CANCEL) d_print_file_error_failed();
  
  do {
    retval=0;
    if ((fd=open(prefs->fxsizesfile,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
      retval=do_write_failed_error_s_with_retry(prefs->fxsizesfile,g_strerror(errno),GTK_WINDOW(rte_window));
      g_free (msg);
    }
    else {
#ifdef IS_MINGW
      setmode(fd, O_BINARY);
#endif
      msg=g_strdup("LiVES generator default sizes file version 2\n");
      mainw->write_failed=FALSE;
      lives_write(fd,msg,strlen(msg),TRUE);
      g_free(msg);
      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(prefs->fxsizesfile,NULL,GTK_WINDOW(rte_window));
      }
      else {
	for (i=0;i<numfx;i++) {
	  if (!write_generator_sizes(fd,i)) {
	    retval=do_write_failed_error_s_with_retry(prefs->fxsizesfile,NULL,GTK_WINDOW(rte_window));
	    break;
	  }
	}
      }
      close(fd);
    }
  } while (retval==LIVES_RETRY);

  if (retval==LIVES_CANCEL) {
    d_print_file_error_failed();
    mainw->write_failed=FALSE;
    return;
  }

  d_print_done();

  return;
}



void load_rte_defs (void) {
  int fd;
  int retval;
  gchar *msg,*msg2;
  void *buf;
  ssize_t bytes;

  if (prefs->fxdefsfile==NULL) {
    prefs->fxdefsfile=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"fxdefs",NULL);
  }

  if (g_file_test(prefs->fxdefsfile,G_FILE_TEST_EXISTS)) {

    do {
      retval=0;
      if ((fd=open(prefs->fxdefsfile,O_RDONLY))==-1) {
	retval=do_read_failed_error_s_with_retry(prefs->fxdefsfile,g_strerror(errno),NULL);
      }
      else {
#ifdef IS_MINGW
	setmode(fd, O_BINARY);
#endif
	mainw->read_failed=FALSE;
	msg2=g_strdup_printf(_("Loading real time effect defaults from %s..."),prefs->fxdefsfile);
	d_print(msg2);
	g_free(msg2);
      
	msg=g_strdup("LiVES filter defaults file version 1.1\n");
	buf=g_malloc(strlen(msg));
	bytes=read(fd,buf,strlen(msg));
      
	if (bytes==strlen(msg)&&!strncmp((gchar *)buf,msg,strlen(msg))) {
	  if (read_filter_defaults(fd)) {
	    d_print_done();
	  }
	  else {
	    d_print_file_error_failed();
	    retval=do_read_failed_error_s_with_retry(prefs->fxdefsfile,NULL,NULL);
	  }
	}
	else {
	  d_print_file_error_failed();
	  if (bytes<strlen(msg)) {
	    retval=do_read_failed_error_s_with_retry(prefs->fxdefsfile,NULL,NULL);
	  }
	}

	close(fd);
	
	g_free(buf);
	g_free(msg);
      }
    } while (retval==LIVES_RETRY);
  }



  if (prefs->fxsizesfile==NULL) {
    prefs->fxsizesfile=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"fxsizes",NULL);
  }

  if (g_file_test(prefs->fxsizesfile,G_FILE_TEST_EXISTS)) {
    do {
      retval=0;
      if ((fd=open(prefs->fxsizesfile,O_RDONLY))==-1) {
	retval=do_read_failed_error_s_with_retry(prefs->fxsizesfile,g_strerror(errno),NULL);
	if (retval==LIVES_CANCEL) return;
      }
      else {
#ifdef IS_MINGW
	setmode(fd, O_BINARY);
#endif
	msg2=g_strdup_printf(_("Loading generator default sizes from %s..."),prefs->fxsizesfile);
	d_print(msg2);
	g_free(msg2);
	
	msg=g_strdup("LiVES generator default sizes file version 2\n");
	buf=g_malloc(strlen(msg));
	bytes=read(fd,buf,strlen(msg));
	if (bytes==strlen(msg)&&!strncmp((gchar *)buf,msg,strlen(msg))) {
	  if (read_generator_sizes(fd)) {
	    d_print_done();
	  }
	  else {
	    d_print_file_error_failed();
	    retval=do_read_failed_error_s_with_retry(prefs->fxsizesfile,NULL,NULL);
	  }
	}
	else {
	  d_print_file_error_failed();
	  if (bytes<strlen(msg)) {
	    retval=do_read_failed_error_s_with_retry(prefs->fxsizesfile,NULL,NULL);
	  }
	}
	close(fd);
	
	g_free(buf);
	g_free(msg);
      }
    } while (retval==LIVES_RETRY);
  }
  
  return;
}


static void check_clear_all_button (void) {
  int modes=rte_getmodespk();
  int i,j;
  boolean hasone=FALSE;

  for (i=0;i<prefs->rte_keys_virtual;i++) {
    for (j=modes-1;j>=0;j--) {
      if (rte_keymode_valid(i+1,j,TRUE)) hasone=TRUE;
    }
  }

  gtk_widget_set_sensitive (GTK_WIDGET(clear_all_button), hasone);
}




static boolean read_perkey_defaults(int kfd, int key, int mode, int version) {
  boolean ret=TRUE;
  int nparams;
  ssize_t bytes=lives_read_le(kfd,&nparams,4,TRUE);

  if (nparams>65536) {
    g_printerr("Too many params, file is probably broken.\n");
    return FALSE;
  }

  if (bytes<sizint) {
    return FALSE;
  }

  if (nparams>0) {
    ret=read_key_defaults(kfd,nparams,key,mode,version);
  }
  return ret;
}




static boolean load_datacons(const gchar *fname, uint8_t **badkeymap) {
  weed_plant_t **ochans,**ichans,**iparams;

  weed_plant_t *filter;

  ssize_t bytes;

  gchar *hashname;

  boolean ret=TRUE;
  boolean is_valid,is_valid2;
  boolean eof=FALSE;

  int kfd;

  int retval;

  int hlen;

  int version,nchans,nparams,nconns,ncconx,npconx;

  int nichans,nochans,niparams,noparams,error;

  int okey,omode,ocnum,opnum,ikey,imode,icnum,ipnum,autoscale;

  int maxmodes=rte_getmodespk();

  register int i,j,k,count;
  

  
  do {
    retval=0;
    if ((kfd=open(fname,O_RDONLY))==-1) {
      retval=do_read_failed_error_s_with_retry(fname,g_strerror(errno),NULL);
    }
    else {
#ifdef IS_MINGW
      setmode(fd, O_BINARY);
#endif
      mainw->read_failed=FALSE;

      bytes=lives_read_le(kfd,&version,4,TRUE);
      if (bytes<4) {
	eof=TRUE;
	break;
      }

      bytes=lives_read_le(kfd,&ncconx,4,TRUE);
      if (bytes<4) {
	eof=TRUE;
	break;
      }

      for (count=0;count<ncconx;count++) {
	is_valid=TRUE;

	bytes=lives_read_le(kfd,&okey,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	if (okey<0||okey>=prefs->rte_keys_virtual) is_valid=FALSE;

	bytes=lives_read_le(kfd,&omode,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	  
	bytes=lives_read_le(kfd,&hlen,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}
	  
	hashname=(gchar *)g_try_malloc(hlen+1);

	if (hashname==NULL) {
	  eof=TRUE;
	  break;
	}

	bytes=read(kfd,hashname,hlen);
	if (bytes<hlen) {
	  eof=TRUE;
	  g_free(hashname);
	  break;
	}
	  
	memset(hashname+hlen,0,1);
	  
	  
	// if we had bad/missing fx, adjust the omode value
	for (i=0;i<omode;i++) omode-=badkeymap[okey][omode];

	if (omode<0||omode>maxmodes) is_valid=FALSE;


	if (is_valid) {
	  int fidx=rte_keymode_get_filter_idx(okey+1,omode);
	  if (fidx==-1) is_valid=FALSE;
	  else {
	    gchar *hashname2=make_weed_hashname(fidx,TRUE,FALSE);
	    if (strcmp(hashname,hashname2)) is_valid=FALSE;
	    g_free(hashname2);
	    if (!is_valid) {
	      hashname2=make_weed_hashname(fidx,TRUE,TRUE);
	      if (!strcmp(hashname,hashname2)) is_valid=TRUE;
	      g_free(hashname2);
	    }
	  }
	}

	g_free(hashname);

	bytes=lives_read_le(kfd,&nchans,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	for (i=0;i<nchans;i++) {
	  is_valid2=is_valid;

	  bytes=lives_read_le(kfd,&ocnum,4,TRUE);
	  if (bytes<4) {
	    eof=TRUE;
	    break;
	  }

	  // check ocnum
	  filter=rte_keymode_get_filter(okey+1,omode);
	  nochans=weed_leaf_num_elements(filter,"out_channel_templates");
	  if (ocnum>=nochans) is_valid2=FALSE;
	  else {
	    ochans=weed_get_plantptr_array(filter,"out_channel_templates",&error);
	    if (!has_alpha_palette(ochans[ocnum])) is_valid2=FALSE;
	    weed_free(ochans);
	  }

	  bytes=lives_read_le(kfd,&nconns,4,TRUE);
	  if (bytes<4) {
	    eof=TRUE;
	    break;
	  }

	  for (j=0;j<nconns;j++) {
	    bytes=lives_read_le(kfd,&ikey,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    bytes=lives_read_le(kfd,&imode,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    bytes=lives_read_le(kfd,&hlen,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }
	      
	    hashname=(gchar *)g_try_malloc(hlen+1);

	    if (hashname==NULL) {
	      eof=TRUE;
	      break;
	    }

	    bytes=read(kfd,hashname,hlen);
	    if (bytes<hlen) {
	      eof=TRUE;
	      g_free(hashname);
	      break;
	    }
	      
	    memset(hashname+hlen,0,1);
	  
	      
	    // if we had bad/missing fx, adjust the omode value
	    for (k=0;k<imode;k++) imode-=badkeymap[ikey][imode];

	    if (imode<0||imode>maxmodes) is_valid2=FALSE;


	    if (is_valid2) {
	      int fidx=rte_keymode_get_filter_idx(ikey+1,imode);
	      if (fidx==-1) is_valid2=FALSE;
	      else {
		gchar *hashname2=make_weed_hashname(fidx,TRUE,FALSE);
		if (strcmp(hashname,hashname2)) is_valid2=FALSE;
		g_free(hashname2);
		if (!is_valid2) {
		  hashname2=make_weed_hashname(fidx,TRUE,TRUE);
		  if (!strcmp(hashname,hashname2)) is_valid2=TRUE;
		  g_free(hashname2);
		}
	      }
	    }

	    g_free(hashname);

	    bytes=lives_read_le(kfd,&icnum,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    // check icnum
	    filter=rte_keymode_get_filter(ikey+1,imode);
	    nichans=weed_leaf_num_elements(filter,"in_channel_templates");
	    if (icnum>=nichans) is_valid2=FALSE;
	    else {
	      ichans=weed_get_plantptr_array(filter,"in_channel_templates",&error);
	      if (!has_alpha_palette(ichans[icnum])) is_valid2=FALSE;
	      weed_free(ichans);
	    }



	    if (is_valid2) cconx_add_connection(okey,omode,ocnum,ikey,imode,icnum);



	  }

	  if (eof) break;


	}

	if (eof) break;


      }

      if (eof) break;

      // params



      bytes=lives_read_le(kfd,&npconx,4,TRUE);
      if (bytes<4) {
	eof=TRUE;
	break;
      }

      for (count=0;count<npconx;count++) {
	is_valid=TRUE;

	bytes=lives_read_le(kfd,&okey,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	if (okey<0||okey>=prefs->rte_keys_virtual) is_valid=FALSE;

	bytes=lives_read_le(kfd,&omode,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	  
	bytes=lives_read_le(kfd,&hlen,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	hashname=(gchar *)g_try_malloc(hlen+1);

	if (hashname==NULL) {
	  eof=TRUE;
	  break;
	}

	bytes=read(kfd,hashname,hlen);
	if (bytes<hlen) {
	  eof=TRUE;
	  g_free(hashname);
	  break;
	}
	  
	memset(hashname+hlen,0,1);
	  
	  
	// if we had bad/missing fx, adjust the omode value
	for (i=0;i<omode;i++) omode-=badkeymap[okey][omode];

	if (omode<0||omode>maxmodes) is_valid=FALSE;


	if (is_valid) {
	  int fidx=rte_keymode_get_filter_idx(okey+1,omode);
	  if (fidx==-1) is_valid=FALSE;
	  else {
	    gchar *hashname2=make_weed_hashname(fidx,TRUE,FALSE);
	    if (strcmp(hashname,hashname2)) is_valid=FALSE;
	    g_free(hashname2);
	    if (!is_valid) {
	      hashname2=make_weed_hashname(fidx,TRUE,TRUE);
	      if (!strcmp(hashname,hashname2)) is_valid=TRUE;
	      g_free(hashname2);
	    }
	  }
	}

	g_free(hashname);

	bytes=lives_read_le(kfd,&nparams,4,TRUE);
	if (bytes<4) {
	  eof=TRUE;
	  break;
	}

	for (i=0;i<nparams;i++) {
	  is_valid2=is_valid;

	  bytes=lives_read_le(kfd,&opnum,4,TRUE);
	  if (bytes<4) {
	    eof=TRUE;
	    break;
	  }

	  // check opnum
	  filter=rte_keymode_get_filter(okey+1,omode);
	  noparams=weed_leaf_num_elements(filter,"out_parameter_templates");
	  if (opnum>=noparams) is_valid2=FALSE;

	  bytes=lives_read_le(kfd,&nconns,4,TRUE);
	  if (bytes<4) {
	    eof=TRUE;
	    break;
	  }

	  for (j=0;j<nconns;j++) {
	    bytes=lives_read_le(kfd,&ikey,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    bytes=lives_read_le(kfd,&imode,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    bytes=lives_read_le(kfd,&hlen,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }
	      
	    hashname=(gchar *)g_try_malloc(hlen+1);

	    if (hashname==NULL) {
	      eof=TRUE;
	      break;
	    }

	    bytes=read(kfd,hashname,hlen);
	    if (bytes<hlen) {
	      eof=TRUE;
	      g_free(hashname);
	      break;
	    }
	      
	    memset(hashname+hlen,0,1);
	  
	    // if we had bad/missing fx, adjust the omode value
	    for (k=0;k<imode;k++) imode-=badkeymap[ikey][imode];

	    if (imode<0||imode>maxmodes) is_valid2=FALSE;

	    if (is_valid2) {
	      int fidx=rte_keymode_get_filter_idx(ikey+1,imode);
	      if (fidx==-1) is_valid2=FALSE;
	      else {
		gchar *hashname2=make_weed_hashname(fidx,TRUE,FALSE);
		if (strcmp(hashname,hashname2)) is_valid2=FALSE;
		g_free(hashname2);
		if (!is_valid2) {
		  hashname2=make_weed_hashname(fidx,TRUE,TRUE);
		  if (!strcmp(hashname,hashname2)) is_valid2=TRUE;
		  g_free(hashname2);
		}
	      }
	    }

	    g_free(hashname);

	    bytes=lives_read_le(kfd,&ipnum,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    // check ipnum
	    filter=rte_keymode_get_filter(ikey+1,imode);
	    niparams=weed_leaf_num_elements(filter,"in_parameter_templates");
	    if (ipnum>=niparams) is_valid2=FALSE;
	    else {
	      iparams=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
	      if (weed_plant_has_leaf(iparams[ipnum],"host_internal_connection")) is_valid2=FALSE;
	      weed_free(iparams);
	    }

	    bytes=lives_read_le(kfd,&autoscale,4,TRUE);
	    if (bytes<4) {
	      eof=TRUE;
	      break;
	    }

	    if (is_valid2) {
	      pconx_add_connection(okey,omode,opnum,ikey,imode,ipnum,autoscale);
	    }


	  }

	  if (eof) break;


	}

	if (eof) break;


      }




      close(kfd);
	
    }
  } while (retval==LIVES_RETRY);

  if (retval==LIVES_CANCEL) {
    d_print_cancelled();
    return FALSE;
  }

  return ret;
}



boolean on_load_keymap_clicked (GtkButton *button, gpointer user_data) {
  // show file errors at this level
  FILE *kfile=NULL;

  GList *list=NULL,*new_list=NULL;

  size_t linelen;
  ssize_t bytes;

  gchar buff[65536];
  gchar *msg,*tmp;
  gchar *whole=g_strdup (""),*whole2;
  gchar *hashname,*hashname_new=NULL;

  gchar *line=NULL;
  gchar *whashname;

  gchar *keymap_file=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap",NULL);
  gchar *keymap_file2=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap2",NULL);
  gchar *keymap_file3=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,"default.keymap3",NULL);

  int *def_modes;
  uint8_t **badkeymap;

  boolean notfound=FALSE;
  boolean has_error=FALSE;
  boolean eof=FALSE;

  int modes=rte_getmodespk();
  int kfd=-1;
  int version;
  int hlen;
  int retval;

  int key,mode;
  int update=0;

  register int i;

  def_modes=(int *)g_malloc(prefs->rte_keys_virtual*sizint);
  for (i=0;i<prefs->rte_keys_virtual;i++) def_modes[i]=-1;

  if (g_file_test(keymap_file2,G_FILE_TEST_EXISTS)) {
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

  do {
    retval=0;

    if (keymap_file2!=NULL) {
      if ((kfd=open(keymap_file,O_RDONLY))==-1) has_error=TRUE;
#ifdef IS_MINGW
      else {
	setmode(kfd, O_BINARY);
      }
#endif

    }
    else {
      if (!(kfile=fopen (keymap_file,"r"))) {
	has_error=TRUE;
      }
    }

    if (has_error) {
      msg=g_strdup_printf (_("\n\nUnable to read from keymap file\n%s\nError code %d\n"),keymap_file,errno);
      retval=do_abort_cancel_retry_dialog(msg,GTK_WINDOW(rte_window));
      g_free (msg);
   
      if (retval==LIVES_CANCEL) {
	g_free(keymap_file);
	d_print_file_error_failed();
	g_free(def_modes);
	return FALSE;
      }
    }
  } while (retval==LIVES_RETRY);

  on_clear_all_clicked(NULL,user_data);

  if (ca_canc) {
    // user cancelled
    mainw->error=FALSE;
    d_print_cancelled();
    g_free(def_modes);
    return FALSE;
  }


  badkeymap=(uint8_t **)g_malloc(prefs->rte_keys_virtual*sizeof(uint8_t *));
  for (i=0;i<prefs->rte_keys_virtual;i++) {
    badkeymap[i]=(uint8_t *)lives_calloc(modes,1);
  }


  if (keymap_file2==NULL) {
    // version 1 file
    while (fgets(buff,65536,kfile)) {
      if (strlen(buff)) {
	line=(g_strstrip(buff));
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

    if (!strcmp((gchar *)g_list_nth_data(list,0),"LiVES keymap file version 2")||
	!strcmp((gchar *)g_list_nth_data(list,0),"LiVES keymap file version 1")) update=1;
    if (!strcmp((gchar *)g_list_nth_data(list,0),"LiVES keymap file version 3")) update=2;
  }
  else {
    // newer style

    // read version
    bytes=lives_read_le(kfd,&version,4,TRUE);
    if (bytes<sizint) {
      eof=TRUE;
    }
  }

  g_free (whole);

  for (i=1;(keymap_file2==NULL&&i<g_list_length(list))||(keymap_file2!=NULL&&!eof);i++) {
    gchar **array;

    if (keymap_file2==NULL) {
      // old style

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
      // newer style

      // file format is: (4 bytes int)key(4 bytes int)hlen(hlen bytes)hashname

      //read key and hashname
      bytes=lives_read_le(kfd,&key,4,TRUE);
      if (bytes<4) {
	eof=TRUE;
	break;
      }

      bytes=lives_read_le(kfd,&hlen,4,TRUE);
      if (bytes<4) {
	eof=TRUE;
	break;
      }

      hashname=(gchar *)g_try_malloc(hlen+1);

      if (hashname==NULL) {
	eof=TRUE;
	break;
      }

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
      LIVES_ERROR(tmp);
      g_free(tmp);
      notfound=TRUE;
      g_free(hashname);
      if (keymap_file2!=NULL) {
	// read param defaults
	if (!read_perkey_defaults(kfd,-1,-1,version)) break; // file read error
      }
      continue;
    }

    def_modes[key-1]++;

    if (strncmp(hashname,"Weed",4)||strlen(hashname)<5) {
      d_print((tmp=g_strdup_printf(_("Invalid effect %s in %s\n"),hashname,keymap_file)));
      LIVES_ERROR(tmp);
      g_free(tmp);
      notfound=TRUE;
      g_free(hashname);
      badkeymap[key-1][def_modes[key-1]]++;
      if (keymap_file2!=NULL) {
	// read param defaults
	if (!read_perkey_defaults(kfd,-1,-1,version)) break; // file read error
      }
      def_modes[key-1]--;
      continue;
    }

    // ignore "Weed"
    whashname=hashname+4;

    if ((mode=weed_add_effectkey(key,whashname,TRUE))==-1) {
      // could not locate effect
      d_print((tmp=g_strdup_printf(_("Unknown effect %s in %s\n"),whashname,keymap_file)));
      LIVES_ERROR(tmp);
      g_free(tmp);
      notfound=TRUE;
      g_free(hashname);
      badkeymap[key-1][def_modes[key-1]]++;
      if (keymap_file2!=NULL) {
	// read param defaults
	if (!read_perkey_defaults(kfd,-1,-1,version)) break; // file read error
      }
      def_modes[key-1]--;
      continue;
    }

    g_free(hashname);

    if (mode==-2){
      d_print((tmp=g_strdup_printf
	       (_("This version of LiVES cannot mix generators/non-generators on the same key (%d) !\n"),key)));
      LIVES_ERROR(tmp);
      g_free(tmp);
      badkeymap[key-1][def_modes[key-1]]++;
      if (keymap_file2!=NULL) {
	// read param defaults
	if (!read_perkey_defaults(kfd,-1,-1,version)) break; // file read error
      }
      def_modes[key-1]--;
      continue;
    }
    if (mode==-3){
      d_print((tmp=g_strdup_printf(_("Too many effects bound to key %d.\n"),key)));
      LIVES_ERROR(tmp);
      g_free(tmp);
      if (keymap_file2!=NULL) {
	// read param defaults
	if (!read_perkey_defaults(kfd,-1,-1,version)) break; // file read error
      }
      def_modes[key-1]--;
      continue;
    }
    if (rte_window!=NULL) {
      gtk_entry_set_text (GTK_ENTRY(combo_entries[(key-1)*modes+mode]),(tmp=rte_keymode_get_filter_name(key,mode)));
      g_free(tmp);
      type_label_set_text(key-1,mode);
    }

    if (keymap_file2!=NULL) {
      // read param defaults
      if (!read_perkey_defaults(kfd,key-1,def_modes[key-1],version)) break; // file read error
    }
  }


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
    if (kfd!=-1) close(kfd);
    d_print_done();
  }

  if (update==0) {
    if (g_file_test(keymap_file3,G_FILE_TEST_EXISTS)) {

      msg=g_strdup_printf(_("Loading data connection map from %s..."),keymap_file3);
      d_print(msg);
      g_free(msg);

      if (load_datacons(keymap_file3,badkeymap)) d_print_done();
    }
    
    if (mainw->is_ready) {
      check_clear_all_button();
      if (notfound) do_warning_dialog_with_check_transient(_("\n\nSome effects could not be located.\n\n"),
							   0,GTK_WINDOW(rte_window));
    }
    else load_rte_defs(); // file errors shown inside

  }

  for (i=0;i<prefs->rte_keys_virtual;i++) {
    g_free(badkeymap[i]);
  }

  g_free(badkeymap);

  g_free(keymap_file); // frees keymap_file2 if applicable

  g_free(keymap_file3);

  g_free(def_modes);

  return FALSE;
}




void on_rte_info_clicked (GtkButton *button, gpointer user_data) {
  weed_plant_t *filter;

  GtkWidget *rte_info_window;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *textview;

  GtkWidget *hbuttonbox;
  GtkWidget *ok_button;

  gchar *filter_name;
  gchar *filter_author;
  gchar *filter_extra_authors=NULL;
  gchar *filter_description;
  gchar *tmp;
  gchar *type;
  gchar *plugin_name;

  int filter_version;
  int weed_error;

  int key_mode=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(key_mode/modes);
  int mode=key_mode-key*modes;


  ////////////////////////

  if (!rte_keymode_valid(key+1,mode,TRUE)) return;

  type=rte_keymode_get_type(key+1,mode);

  plugin_name=rte_keymode_get_plugin_name(key+1,mode);
  filter=rte_keymode_get_filter(key+1,mode);
  filter_name=weed_get_string_value(filter,"name",&weed_error);
  filter_author=weed_get_string_value(filter,"author",&weed_error);
  if (weed_plant_has_leaf(filter,"extra_authors")) filter_extra_authors=weed_get_string_value(filter,"extra_authors",&weed_error);
  if (weed_plant_has_leaf(filter,"description")) filter_description=weed_get_string_value(filter,"description",&weed_error);
  else filter_description=g_strdup(_("No Description"));

  filter_version=weed_get_int_value(filter,"version",&weed_error);

  rte_info_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (rte_info_window), g_strdup_printf(_("LiVES: Information for %s"),filter_name));
  lives_widget_set_bg_color(rte_info_window, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);

  gtk_container_set_border_width (GTK_CONTAINER (rte_info_window), widget_opts.border_width);
  gtk_window_set_transient_for(GTK_WINDOW(rte_info_window),GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))));
  gtk_window_set_position (GTK_WINDOW (rte_info_window), GTK_WIN_POS_CENTER_ALWAYS);
  gtk_window_set_default_size (GTK_WINDOW (rte_info_window), RTE_INFO_WIDTH, RTE_INFO_HEIGHT);

  vbox = lives_vbox_new (FALSE, widget_opts.packing_height*2);
  gtk_container_add (GTK_CONTAINER (rte_info_window), vbox);

  label = lives_standard_label_new ((tmp=g_strdup_printf(_("Effect name: %s"),filter_name)));
  g_free(tmp);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = lives_standard_label_new ((tmp=g_strdup_printf(_("Type: %s"),type)));
  g_free(tmp);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = lives_standard_label_new ((tmp=g_strdup_printf(_("Plugin name: %s"),plugin_name)));
  g_free(tmp);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  label = lives_standard_label_new ((tmp=g_strdup_printf(_("Author: %s"),filter_author)));
  g_free(tmp);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  if (filter_extra_authors!=NULL) {
    label = lives_standard_label_new ((tmp=g_strdup_printf(_("and: %s"),filter_extra_authors)));
    g_free(tmp);
    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);
  }

  label = lives_standard_label_new ((tmp=g_strdup_printf(_("Version: %d"),filter_version)));
  g_free(tmp);
  gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, FALSE, 0);

  hbox = lives_hbox_new (FALSE, widget_opts.packing_width);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, FALSE, 0);

  label = lives_standard_label_new (_("Description: "));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

  textview = gtk_text_view_new ();

  if (palette->style&STYLE_1) {
    lives_widget_set_text_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->normal_fore);
    lives_widget_set_base_color(textview, LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_text_view_set_editable (GTK_TEXT_VIEW (textview), FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview), GTK_WRAP_WORD);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (textview), FALSE);
  
  text_view_set_text (LIVES_TEXT_VIEW(textview), filter_description,-1);
  gtk_box_pack_start (GTK_BOX (hbox), textview, TRUE, TRUE, 0);
  
  hbuttonbox = lives_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (vbox), hbuttonbox, TRUE, TRUE, 0);

  ok_button = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (ok_button);

  gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);
  lives_widget_set_can_focus_and_default (ok_button);
  gtk_widget_grab_default (ok_button);

#if !GTK_CHECK_VERSION(3,0,0)
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
#else
  gtk_widget_set_size_request(ok_button,DEF_BUTTON_WIDTH*4,-1);
#endif
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);


  g_signal_connect (GTK_OBJECT (ok_button), "clicked",
		    G_CALLBACK (lives_general_button_clicked),
		    NULL);

  g_free(filter_name);
  g_free(filter_author);
  if (filter_extra_authors!=NULL) g_free(filter_extra_authors);
  if (weed_plant_has_leaf(filter,"description")) g_free(filter_description);
  else weed_free(filter_description);
  g_free(plugin_name);
  g_free(type);

  gtk_widget_show_all(rte_info_window);

}





void on_clear_clicked (GtkButton *button, gpointer user_data) {
  // this is for the "delete" buttons, c.f. clear_all

  gint idx=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(idx/modes);
  int mode=idx-key*modes;

  int newmode;

  register int i;

  weed_delete_effectkey (key+1,mode);

  pconx_delete(-1,0,0,key,mode,-1);
  pconx_delete(key,mode,-1,-1,0,0);

  cconx_delete(-1,0,0,key,mode,-1);
  cconx_delete(key,mode,-1,-1,0,0);


  newmode=rte_key_getmode(key+1);
  g_signal_handler_block(mode_radios[key*modes+newmode],mode_ra_fns[key*modes+newmode]);
  rtew_set_mode_radio(key,newmode);
  g_signal_handler_unblock(mode_radios[key*modes+newmode],mode_ra_fns[key*modes+newmode]);
    
  for (i=mode;i<rte_getmodespk()-1;i++) {
    idx=key*modes+i;
    gtk_entry_set_text (GTK_ENTRY(combo_entries[idx]),gtk_entry_get_text(GTK_ENTRY(combo_entries[idx+1])));
    type_label_set_text(key,i);
    pconx_remap_mode(key,i+1,i);
    cconx_remap_mode(key,i+1,i);
  }
  idx++;
  gtk_entry_set_text (GTK_ENTRY(combo_entries[idx]),"");
  type_label_set_text(key,i);

  if (!rte_keymode_valid(key+1,0,TRUE)) rtew_set_keych(key,FALSE);

  check_clear_all_button();

}


static void on_datacon_clicked (GtkButton *button, gpointer user_data) {
  gint idx=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(idx/modes);
  int mode=idx-key*modes;

  //if (datacon_dialog!=NULL) on_datacon_cancel_clicked(NULL,NULL);

  datacon_dialog=make_datacon_window(key,mode);

}


static void on_params_clicked (GtkButton *button, gpointer user_data) {
  gint idx=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(idx/modes);
  int mode=idx-key*modes;

  weed_plant_t *inst;
  lives_rfx_t *rfx;

  if ((inst=rte_keymode_get_instance(key+1,mode))==NULL) {
    weed_plant_t *filter=rte_keymode_get_filter(key+1,mode);
    if (filter==NULL) return;
    inst=weed_instance_from_filter(filter);
    apply_key_defaults(inst,key,mode);
  }
  else {
    int error;
    weed_plant_t *ninst=inst;
    do {
      weed_instance_ref(ninst);
    } while (weed_plant_has_leaf(ninst,"host_next_instance")&&(ninst=weed_get_plantptr_value(ninst,"host_next_instance",&error))!=NULL);
  }


  if (fx_dialog[1]!=NULL) {
    rfx=(lives_rfx_t *)g_object_get_data (G_OBJECT (fx_dialog[1]),"rfx");
    gtk_widget_destroy(fx_dialog[1]);
    on_paramwindow_cancel_clicked2(NULL,rfx);
  }

  rfx=weed_to_rfx(inst,FALSE);
  rfx->min_frames=-1;
  keyw=key;
  modew=mode;
  on_render_fx_pre_activate(NULL,rfx);

  // record the key so we know whose parameters to record later
  weed_set_int_value((weed_plant_t *)rfx->source,"host_hotkey",key);

  g_object_set_data (G_OBJECT (fx_dialog[1]),"key",GINT_TO_POINTER (key));
  g_object_set_data (G_OBJECT (fx_dialog[1]),"mode",GINT_TO_POINTER (mode));
  g_object_set_data (G_OBJECT (fx_dialog[1]),"rfx",rfx);
}


static boolean on_rtew_delete_event (GtkWidget *widget, GdkEvent *event, gpointer user_data) {

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
  g_free(conx_buttons);
  g_free(clear_buttons);
  rte_window=NULL;
  return FALSE;
}


static void on_rtew_ok_clicked (GtkButton *button, gpointer user_data) {
  lives_general_button_clicked(button,NULL);
  on_rtew_delete_event (NULL,NULL,NULL);
}




static void do_mix_error(void) {
  do_error_dialog_with_check_transient(
				       _("\n\nThis version of LiVES does not allowing mixing of generators and non-generators on the same key.\n\n"),
				       FALSE,0,GTK_WINDOW(rte_window));
  return;
}



enum {
  NAME_TYPE_COLUMN,
  NAME_COLUMN,
  HASH_COLUMN,
  NUM_COLUMNS
};



void fx_changed (GtkComboBox *combo, gpointer user_data) {
  GtkTreeIter iter1;
  GtkTreeModel *model;

  weed_plant_t *filter;

  gchar *txt;
  gchar *tmp;
  gchar *hashname1;
  gchar *hashname2=(gchar *)g_object_get_data(G_OBJECT(combo),"hashname");

  gint key_mode=GPOINTER_TO_INT(user_data);
  int modes=rte_getmodespk();
  int key=(int)(key_mode/modes);
  int mode=key_mode-key*modes;
  gint idx=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo),"idx"));

  int error;

  register int i;

  if (gtk_combo_box_get_active(combo)==-1) return; // -1 is returned after we set our own text (without the type)

  gtk_combo_box_get_active_iter(combo,&iter1);
  model=gtk_combo_box_get_model(combo);

  gtk_tree_model_get(model,&iter1,HASH_COLUMN,&hashname1,-1);

  if (!strcmp(hashname1,hashname2)) {
    g_free(hashname1);
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

    if (error==-2) do_mix_error();
    if (error==-1) {
      d_print((tmp=g_strdup_printf(_("LiVES could not locate the effect %s.\n"),rte_keymode_get_filter_name(key+1,mode))));
      g_free(tmp);
    }
    return;
  }

  // set parameters button sensitive/insensitive
  filter=rte_keymode_get_filter(key+1,mode);
  if (num_in_params(filter,TRUE,TRUE)>0) gtk_widget_set_sensitive(param_buttons[idx],TRUE);
  else gtk_widget_set_sensitive(param_buttons[idx],FALSE);

  if ((weed_plant_has_leaf(filter,"out_parameter_templates")&&weed_get_plantptr_value(filter,"out_parameter_templates",&error)!=NULL)||num_alpha_channels(filter,TRUE)>1) gtk_widget_set_sensitive(conx_buttons[idx],TRUE);
  else gtk_widget_set_sensitive(conx_buttons[idx],FALSE);

  // prevents a segfault
  gtk_combo_box_get_active_iter(combo,&iter1);
  model=gtk_combo_box_get_model(combo);

  gtk_tree_model_get(model,&iter1,NAME_COLUMN,&txt,-1);
  gtk_entry_set_text (GTK_ENTRY (combo_entries[key_mode]),txt);
  g_free(txt);
  g_free(hashname2);

  // TODO - g_free on delete
  g_object_set_data(G_OBJECT(combo),"hashname",hashname1);
    
  type_label_set_text(key,mode);

  check_clear_all_button();

  pconx_delete(-1,0,0,key,mode,-1);
  pconx_delete(key,mode,-1,-1,0,0);

  cconx_delete(-1,0,0,key,mode,-1);
  cconx_delete(key,mode,-1,-1,0,0);

}




static LiVESTreeModel *rte_window_fx_model (void) {
  GtkTreeStore *tstore;

  GtkTreeIter iter1,iter2;

  // fill names of our effects
  int fx_idx=0;

  GList *list=name_type_list;

  int error;

  gchar *pkg=NULL,*pkgstring,*fxname;

  tstore=gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  while (list!=NULL) {
    weed_plant_t *filter=get_weed_filter(weed_get_idx_for_hashname((gchar *)g_list_nth_data(hash_list,fx_idx),TRUE));
    int filter_flags=weed_get_int_value(filter,"flags",&error);
    if ((weed_plant_has_leaf(filter,"plugin_unstable")&&weed_get_boolean_value(filter,"plugin_unstable",&error)==
	 WEED_TRUE&&!prefs->unstable_fx)||((enabled_in_channels(filter,FALSE)>1&&
					    !has_video_chans_in(filter,FALSE))||
					   (weed_plant_has_leaf(filter,"host_menu_hide")&&
					    weed_get_boolean_value(filter,"host_menu_hide",&error)==WEED_TRUE)
					   ||(filter_flags&WEED_FILTER_IS_CONVERTER))) {
      list = list->next;
      fx_idx++;
      continue; // skip audio transitions and hidden entries
    }


    fxname=g_strdup((gchar *)g_list_nth_data(name_list,fx_idx));

    if ((pkgstring=strstr(fxname,": "))!=NULL) {
      // package effect
      if (pkg==NULL) {
	pkg=fxname;
	fxname=g_strdup(pkg);
	memset(pkgstring,0,1);
	/* TRANSLATORS: example " - LADSPA plugins -" */
	pkgstring=g_strdup_printf(_(" - %s plugins -"),pkg);
	gtk_tree_store_append (tstore, &iter1, NULL);
	gtk_tree_store_set(tstore,&iter1,NAME_TYPE_COLUMN,pkgstring,NAME_COLUMN,fxname,
			 HASH_COLUMN,g_list_nth_data(hash_list,fx_idx),-1);
	g_free(pkgstring);
      }
      gtk_tree_store_append (tstore, &iter2, &iter1);
      gtk_tree_store_set(tstore,&iter2,NAME_TYPE_COLUMN,list->data,NAME_COLUMN,fxname,
			 HASH_COLUMN,g_list_nth_data(hash_list,fx_idx),-1);
    }
    else {
      if (pkg!=NULL) g_free(pkg);
      pkg=NULL;
      gtk_tree_store_append (tstore, &iter1, NULL);  /* Acquire an iterator */
      gtk_tree_store_set(tstore,&iter1,NAME_TYPE_COLUMN,list->data,NAME_COLUMN,fxname,
			 HASH_COLUMN,g_list_nth_data(hash_list,fx_idx),-1);
      }

    g_free(fxname);

    list = list->next;
    fx_idx++;
  }

  if (pkg!=NULL) g_free(pkg);

  return (LiVESTreeModel *)tstore;
}












GtkWidget * create_rte_window (void) {
  GtkWidget *rte_window;
  GtkWidget *table;
  GtkWidget *hbox;
  GtkWidget *hbox2;

  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *ok_button;
  GtkWidget *top_vbox;
  GtkWidget *hbuttonbox;

  GtkWidget *scrolledwindow;

  GSList *mode_group = NULL;
  GSList *grab_group = NULL;

  GtkAccelGroup *rtew_accel_group;

  LiVESTreeModel *model;

  weed_plant_t *filter;

  gchar *tmp,*tmp2;

  int modes=rte_getmodespk();

  int idx,error;

  int winsize_h;
  int winsize_v;

  int scr_width,scr_height;

  register int i,j;

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
  conx_buttons=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  clear_buttons=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  nlabels=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));
  type_labels=(GtkWidget **)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(GtkWidget *));

  ch_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*sizeof(gulong));
  gr_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*sizeof(gulong));
  mode_ra_fns=(gulong *)g_malloc((prefs->rte_keys_virtual)*modes*sizeof(gulong));

  rte_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  lives_widget_set_bg_color(rte_window, LIVES_WIDGET_STATE_NORMAL, &palette->menu_and_bars);
  gtk_window_set_title (GTK_WINDOW (rte_window), _("LiVES: Real time effect mapping"));
  gtk_window_add_accel_group (GTK_WINDOW (rte_window), mainw->accel_group);

  table = gtk_table_new (prefs->rte_keys_virtual, modes+1, FALSE);

  gtk_table_set_row_spacings (GTK_TABLE (table), 16);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);

  // dummy button for "no grab", we dont show this...there is a button instead
  dummy_radio = gtk_radio_button_new (grab_group);
  grab_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (dummy_radio));

  name_list=weed_get_all_names(1);
  name_type_list=weed_get_all_names(2);
  hash_list=weed_get_all_names(3);

  for (i=0;i<prefs->rte_keys_virtual;i++) {

    hbox = lives_hbox_new (FALSE, 10);
    gtk_table_attach (GTK_TABLE (table), hbox, i, i+1, 0, 1,
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
		      (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);
    
    label = lives_standard_label_new ((tmp=g_strdup_printf(_("Ctrl-%d"),i+1)));
    g_free(tmp);

    gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, FALSE, 0);

    hbox2 = lives_hbox_new (FALSE, 0);

    key_checks[i] = lives_standard_check_button_new (_("Key active"),FALSE,LIVES_BOX(hbox2),NULL);
    
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_checks[i]),mainw->rte&(GU641<<i));

    ch_fns[i]=g_signal_connect_after (GTK_OBJECT (key_checks[i]), "toggled",
                      G_CALLBACK (rte_on_off_callback_hook),GINT_TO_POINTER (i+1));



    hbox2 = lives_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);

    key_grabs[i]=lives_standard_radio_button_new((tmp=g_strdup(_ ("Key grab"))),FALSE,grab_group,LIVES_BOX(hbox2),
						 (tmp2=g_strdup(_("Grab keyboard for this effect key"))));
    g_free(tmp); g_free(tmp2);
    grab_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (key_grabs[i]));
    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(key_grabs[i]),mainw->rte_keys==i);

    gr_fns[i]=g_signal_connect_after (GTK_OBJECT (key_grabs[i]), "toggled",
				      G_CALLBACK (grabkeys_callback_hook),GINT_TO_POINTER (i));

    mode_group=NULL;

    clear_all_button = gtk_button_new_with_mnemonic (_("_Clear all effects"));

    for (j=0;j<modes;j++) {
      idx=i*modes+j;
      hbox = lives_hbox_new (FALSE, 0);
      gtk_table_attach (GTK_TABLE (table), hbox, i, i+1, j+1, j+2,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 12);


      hbox2 = lives_hbox_new (FALSE, 0);
      gtk_box_pack_start (GTK_BOX (hbox), hbox2, FALSE, FALSE, 10);
      
      mode_radios[idx]=lives_standard_radio_button_new(_ ("Mode active"),FALSE,mode_group,LIVES_BOX(hbox2),NULL);
      mode_group = lives_radio_button_get_group (LIVES_RADIO_BUTTON (mode_radios[idx]));

      if (rte_key_getmode(i+1)==j) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(mode_radios[idx]),TRUE);

      mode_ra_fns[idx]=g_signal_connect_after (GTK_OBJECT (mode_radios[idx]), "toggled",
					       G_CALLBACK (rtemode_callback_hook),GINT_TO_POINTER (idx));

      type_labels[idx] = lives_standard_label_new ("");

      info_buttons[idx] = gtk_button_new_with_label (_("Info"));
      param_buttons[idx] = gtk_button_new_with_label (_("Set Parameters"));
      conx_buttons[idx] = gtk_button_new_with_label (_("Set Connections"));
      clear_buttons[idx] = gtk_button_new_with_label (_("Clear"));

      vbox = lives_vbox_new (FALSE, 15);
      gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 13);

      hbox = lives_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      nlabels[idx] = lives_standard_label_new (_("Effect name:"));

      gtk_box_pack_start (GTK_BOX (hbox), nlabels[idx], FALSE, FALSE, 0);

      // create combo entry model
      model=rte_window_fx_model();

      combo = lives_combo_new_with_model (model);

      lives_combo_set_entry_text_column(LIVES_COMBO(combo),NAME_TYPE_COLUMN);

      g_object_set_data (G_OBJECT(combo), "hashname", (gpointer)g_strdup(""));
      g_object_set_data (G_OBJECT(combo), "idx", GINT_TO_POINTER(idx));
      gtk_box_pack_start (GTK_BOX (hbox), combo, TRUE, TRUE, 0);


      combo_entries[idx] = lives_combo_get_entry(LIVES_COMBO(combo));
      
      gtk_entry_set_text (GTK_ENTRY (combo_entries[idx]),(tmp=rte_keymode_get_filter_name(i+1,j)));
      g_free(tmp);
 
      lives_entry_set_editable (LIVES_ENTRY (combo_entries[idx]), FALSE);
      
      hbox = lives_hbox_new (FALSE, 12);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      g_signal_connect(GTK_OBJECT (combo), "changed",
		       G_CALLBACK (fx_changed),GINT_TO_POINTER(i*rte_getmodespk()+j));
      
      g_signal_connect (GTK_OBJECT (info_buttons[idx]), "clicked",
			G_CALLBACK (on_rte_info_clicked),GINT_TO_POINTER (idx));

      g_signal_connect (GTK_OBJECT (clear_buttons[idx]), "clicked",
			G_CALLBACK (on_clear_clicked),GINT_TO_POINTER (idx));

      g_signal_connect (GTK_OBJECT (param_buttons[idx]), "clicked",
			G_CALLBACK (on_params_clicked),GINT_TO_POINTER (idx));

      g_signal_connect (GTK_OBJECT (conx_buttons[idx]), "clicked",
			G_CALLBACK (on_datacon_clicked),GINT_TO_POINTER (idx));
      
      type_label_set_text(i,j);

      gtk_box_pack_start (GTK_BOX (hbox), type_labels[idx], FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), info_buttons[idx], FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), conx_buttons[idx], FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), param_buttons[idx], FALSE, FALSE, 0);
      gtk_box_pack_end (GTK_BOX (hbox), clear_buttons[idx], FALSE, FALSE, 0);

      filter=rte_keymode_get_filter(i+1,j);

      if (num_in_params(filter,TRUE,TRUE)>0) gtk_widget_set_sensitive(param_buttons[idx],TRUE);
      else gtk_widget_set_sensitive(param_buttons[idx],FALSE);

      if ((weed_plant_has_leaf(filter,"out_parameter_templates")&&weed_get_plantptr_value(filter,"out_parameter_templates",&error)!=NULL)||num_alpha_channels(filter,TRUE)>1) gtk_widget_set_sensitive(conx_buttons[idx],TRUE);
      else gtk_widget_set_sensitive(conx_buttons[idx],FALSE);



      }
  }


  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), table);

  gtk_widget_set_size_request (scrolledwindow, winsize_h, winsize_v);

  if (palette->style&STYLE_1) {
    lives_widget_set_bg_color(gtk_bin_get_child (GTK_BIN (scrolledwindow)), LIVES_WIDGET_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);

  top_vbox = lives_vbox_new (FALSE, 15);

  gtk_box_pack_start (GTK_BOX (top_vbox), dummy_radio, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (rte_window), top_vbox);

  hbuttonbox = lives_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (top_vbox), hbuttonbox, FALSE, TRUE, 20);

  gtk_container_add (GTK_CONTAINER (hbuttonbox), clear_all_button);
  lives_widget_set_can_focus_and_default (clear_all_button);

  save_keymap_button = gtk_button_new_with_mnemonic (_("_Save as default keymap"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), save_keymap_button);
  lives_widget_set_can_focus_and_default (save_keymap_button);

  load_keymap_button = gtk_button_new_with_mnemonic (_("_Load default keymap"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), load_keymap_button);
  lives_widget_set_can_focus_and_default (load_keymap_button);

  ok_button = gtk_button_new_with_mnemonic (_("Close _window"));

  gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);
  lives_widget_set_can_focus_and_default (ok_button);

#if !GTK_CHECK_VERSION(3,0,0)
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
#endif


  rtew_accel_group = GTK_ACCEL_GROUP(gtk_accel_group_new ());
  gtk_window_add_accel_group (GTK_WINDOW (rte_window), rtew_accel_group);

  gtk_widget_add_accelerator (ok_button, "activate", rtew_accel_group,
                              LIVES_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0);

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
    gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-
						    lives_widget_get_allocation_width(rte_window))/2;
    gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-
						    lives_widget_get_allocation_height(rte_window))/2;
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


void on_assign_rte_keys_activate (GtkMenuItem *menuitem, gpointer user_data) {
  if (rte_window!=NULL) {
    on_rtew_ok_clicked(GTK_BUTTON(dummy_radio), user_data);
    return;
  }
  
  rte_window=create_rte_window();
  gtk_widget_show (rte_window);
}


void rtew_set_keych (int key, boolean on) {
  g_signal_handler_block(key_checks[key],ch_fns[key]);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(key_checks[key]),on);
  g_signal_handler_unblock(key_checks[key],ch_fns[key]);
}


void rtew_set_keygr (int key) {
  if (key>=0) {
    g_signal_handler_block(key_grabs[key],gr_fns[key]);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(key_grabs[key]),TRUE);
    g_signal_handler_unblock(key_grabs[key],gr_fns[key]);
  }
  else {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dummy_radio),TRUE);
  }
}

void rtew_set_mode_radio (int key, int mode) {
  int modes=rte_getmodespk();
  lives_toggle_button_set_active (LIVES_TOGGLE_BUTTON(mode_radios[key*modes+mode]),TRUE);
}



void redraw_pwindow (int key, int mode) {
  GList *child_list;
  lives_rfx_t *rfx;

  GtkWidget *action_area;

  int keyw=0,modew=0;
  int i;

  if (fx_dialog[1]!=NULL) {
    rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
    if (!rfx->is_template) {
      keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
      modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    }
    if (rfx->is_template||(key==keyw&&mode==modew)) {
      // rip out the contents
      if (mainw->invis==NULL) mainw->invis=lives_vbox_new(FALSE,0);
      child_list=gtk_container_get_children(GTK_CONTAINER(lives_dialog_get_content_area(GTK_DIALOG(fx_dialog[1]))));
      action_area=lives_dialog_get_action_area(LIVES_DIALOG(fx_dialog[1]));
      gtk_container_set_focus_child(GTK_CONTAINER(action_area),NULL);
      for (i=0;i<g_list_length(child_list);i++) {
	GtkWidget *widget=(GtkWidget *)g_list_nth_data(child_list,i);
	if (widget!=action_area) {
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
    make_param_box(GTK_VBOX (lives_dialog_get_content_area(GTK_DIALOG(fx_dialog[1]))),rfx);
    gtk_widget_show_all (lives_dialog_get_content_area(GTK_DIALOG(fx_dialog[1])));
    gtk_widget_queue_draw(fx_dialog[1]);
  }
}


void update_pwindow (int key, int i, GList *list) {
  const weed_plant_t *inst;
  lives_rfx_t *rfx;
  int keyw,modew;

  if (fx_dialog[1]!=NULL) {
    keyw=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
    modew=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    if (key==keyw) {
      if ((inst=rte_keymode_get_instance(key+1,modew))==NULL) return;
      rfx=(lives_rfx_t *)g_object_get_data(G_OBJECT(fx_dialog[1]),"rfx");
      set_param_from_list(list,&rfx->params[i],0,TRUE,TRUE);
    }
  }
}

void rte_set_defs_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gint idx=GPOINTER_TO_INT(user_data);
  weed_plant_t *filter=get_weed_filter(idx);
  lives_rfx_t *rfx;

  if (fx_dialog[1]!=NULL) {
    rfx=(lives_rfx_t *)g_object_get_data (G_OBJECT (fx_dialog[1]),"rfx");
    gtk_widget_destroy(fx_dialog[1]);
    on_paramwindow_cancel_clicked2(NULL,rfx);
  }

  rfx=weed_to_rfx(filter,TRUE);
  rfx->min_frames=-1;
  on_render_fx_pre_activate(NULL,rfx);

}



void rte_set_key_defs (GtkButton *button, lives_rfx_t *rfx) {
  int key,mode;
  if (mainw->textwidget_focus!=NULL) {
    GtkWidget *textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (mainw->textwidget_focus),"textwidget");
    after_param_text_changed(textwidget,rfx);
  }

  if (rfx->num_params>0) {
    key=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
    mode=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    set_key_defaults((weed_plant_t *)rfx->source,key,mode);
  }
}




void rte_set_defs_ok (GtkButton *button, lives_rfx_t *rfx) {
  weed_plant_t **ptmpls,*filter;
  lives_colRGB24_t *rgbp;

  int i;
  int error;

  if (mainw->textwidget_focus!=NULL) {
    GtkWidget *textwidget=(GtkWidget *)g_object_get_data (G_OBJECT (mainw->textwidget_focus),"textwidget");
    after_param_text_changed(textwidget,rfx);
  }

  if (rfx->num_params>0) {
    filter=weed_instance_get_filter((weed_plant_t *)rfx->source,TRUE);
    ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
    for (i=0;i<rfx->num_params;i++) {
      switch (rfx->params[i].type) {
      case LIVES_PARAM_COLRGB24:
	rgbp=(lives_colRGB24_t *)rfx->params[i].value;
	update_weed_color_value(ptmpls[i],i,rgbp->red,rgbp->green,rgbp->blue,0);
	break;
      case LIVES_PARAM_STRING:
	weed_set_string_value(ptmpls[i],"host_default",(gchar *)rfx->params[i].value);
	break;
      case LIVES_PARAM_STRING_LIST:
	weed_set_int_array(ptmpls[i],"host_default",1,(int *)rfx->params[i].value);
	break;
      case LIVES_PARAM_NUM:
	if (weed_leaf_seed_type(ptmpls[i],"default")==WEED_SEED_DOUBLE) weed_set_double_array(ptmpls[i],"host_default",1,(double *)rfx->params[i].value);
	else weed_set_int_array(ptmpls[i],"host_default",1,(int *)rfx->params[i].value);
	break;
      case LIVES_PARAM_BOOL:
	weed_set_boolean_array(ptmpls[i],"host_default",1,(int *)rfx->params[i].value);
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




void rte_reset_defs_clicked (GtkButton *button, lives_rfx_t *rfx) {
  weed_plant_t **ptmpls,**inp,**xinp;
  weed_plant_t **ctmpls;

  weed_plant_t *filter,*inst;

  GList *child_list;

  GtkWidget *pbox,*fxdialog,*cancelbutton,*action_area;

  int error;
  int nchans;

  int poffset=0,ninpar,x;

  boolean is_generic_defs=FALSE;
  boolean add_pcons=FALSE;

  register int i;

  cancelbutton=(GtkWidget *)g_object_get_data(G_OBJECT(button),"cancelbutton");

  if (cancelbutton!=NULL) is_generic_defs=TRUE;

  inst=(weed_plant_t *)rfx->source;

  filter=weed_instance_get_filter(inst,TRUE);

  if (rfx->num_params>0) {

    if (is_generic_defs) {
      // for generic, reset from plugin supplied defs
      ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
      for (i=0;i<rfx->num_params;i++) {
	if (weed_plant_has_leaf(ptmpls[i],"host_default")) weed_leaf_delete(ptmpls[i],"host_default");
      }
      weed_free(ptmpls);
    }

    inp=weed_params_create(filter,TRUE);

  resetdefs1:
    filter=weed_instance_get_filter(inst,FALSE);

    // reset params back to default defaults
    weed_in_parameters_free(inst);
    
    ninpar=num_in_params(filter,FALSE,FALSE);
    if (ninpar==0) xinp=NULL;

    xinp=(weed_plant_t **)g_malloc((ninpar+1)*sizeof(weed_plant_t *));
    x=0;
    for (i=poffset;i<poffset+ninpar;i++) xinp[x++]=inp[i];
    xinp[x]=NULL;
    poffset+=ninpar;

    weed_set_plantptr_array(inst,"in_parameters",weed_flagset_array_count(xinp,TRUE),xinp);
    weed_free(xinp);

    if (weed_plant_has_leaf(inst,"host_next_instance")) {
      // handle compound fx
      inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
      add_pcons=TRUE;
      goto resetdefs1;
    }

    weed_free(inp);

    inst=(weed_plant_t *)rfx->source;
    filter=weed_instance_get_filter(inst,TRUE);

    if (add_pcons) {
      add_param_connections(inst);
    }

    rfx_params_free(rfx);
    g_free(rfx->params);
    
    rfx->params=weed_params_to_rfx(rfx->num_params,inst,FALSE);
  }

  if (is_generic_defs) {
    if (weed_plant_has_leaf(filter,"host_fps")) weed_leaf_delete(filter,"host_fps");
    
    if (weed_plant_has_leaf(filter,"out_channel_templates")) {
      ctmpls=weed_get_plantptr_array(filter,"out_channel_templates",&error);
      nchans=weed_leaf_num_elements(filter,"out_channel_templates");
      for (i=0;i<nchans;i++) {
	if (weed_plant_has_leaf(ctmpls[i],"host_width")) weed_leaf_delete(ctmpls[i],"host_width");
	if (weed_plant_has_leaf(ctmpls[i],"host_height")) weed_leaf_delete(ctmpls[i],"host_height");
      }
    }
  }
  else {
    gint key=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"key"));
    gint mode=GPOINTER_TO_INT (g_object_get_data (G_OBJECT (fx_dialog[1]),"mode"));
    set_key_defaults(inst,key,mode);
  }


  fxdialog=gtk_widget_get_toplevel(GTK_WIDGET(button));
  pbox=lives_dialog_get_content_area(GTK_DIALOG(fxdialog));

  // redraw the window

  if (mainw->invis==NULL) mainw->invis=lives_vbox_new(FALSE,0);
  child_list=gtk_container_get_children(GTK_CONTAINER(lives_dialog_get_content_area(GTK_DIALOG(fxdialog))));

  action_area=lives_dialog_get_action_area(LIVES_DIALOG (fxdialog));

  for (i=0;i<g_list_length(child_list);i++) {
    GtkWidget *widget=(GtkWidget *)g_list_nth_data(child_list,i);
    if (widget!=action_area) {
      // we have to do this, because using gtk_widget_destroy() here 
      // can causes a crash [bug in gtk+ ???]
      gtk_widget_reparent (widget,mainw->invis);
    }
  }
  
  if (cancelbutton!=NULL) gtk_widget_set_sensitive(cancelbutton,FALSE);

  make_param_box(GTK_VBOX (pbox), rfx);
  gtk_widget_show_all(pbox);

  gtk_widget_queue_draw(fxdialog);

}


void load_default_keymap(void) {
  // called on startup
  gchar *dir=g_build_filename(capable->home_dir,LIVES_CONFIG_DIR,NULL);
  gchar *keymap_file=g_build_filename(dir,"default.keymap",NULL);
  gchar *keymap_template=g_build_filename(prefs->prefix_dir,DATA_DIR,"default.keymap",NULL);
  gchar *com,*tmp;

  int retval;

  threaded_dialog_spin();

  do {
    retval=0;
    if (!g_file_test (keymap_file, G_FILE_TEST_EXISTS)) {

      if (!g_file_test(dir,G_FILE_TEST_IS_DIR)) {
	g_mkdir_with_parents(dir,S_IRWXU);
      }
	
#ifndef IS_MINGW
      com=g_strdup_printf("/bin/cp \"%s\" \"%s\"",keymap_template,keymap_file);
#else
      com=g_strdup_printf("cp.exe \"%s\" \"%s\"",keymap_template,keymap_file);
#endif

      lives_system(com,TRUE); // allow this to fail - we will check for errors below
      g_free(com);
    }
    if (!g_file_test (keymap_file, G_FILE_TEST_EXISTS)) {
      // give up
      d_print((tmp=g_strdup_printf
	       (_("Unable to create default keymap file: %s\nPlease make sure your home directory is writable.\n"),
		keymap_file)));

      retval=do_abort_cancel_retry_dialog(tmp,NULL);

      g_free(tmp);

      if (retval==LIVES_CANCEL) {
	g_free(keymap_file);
	g_free(keymap_template);
	g_free(dir);

	threaded_dialog_spin();
	return;
      }
    }
  } while (retval==LIVES_RETRY);

  on_load_keymap_clicked(NULL,NULL);

  g_free(keymap_file);
  g_free(keymap_template);
  g_free(dir);
  threaded_dialog_spin();
}
