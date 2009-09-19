// utils.c
// LiVES
// (c) G. Finch 2003 - 2009 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "support.h"
#include "audio.h"

static gboolean  omute,  osepwin,  ofs,  ofaded,  odouble;


// special allocators which avoid free()ing mainw->do_not_free...this is necessary to "hack" into gdk-pixbuf
void lives_free(gpointer ptr) {
  (*mainw->free_fn)(ptr);
}

void lives_free_with_check(gpointer ptr) {
  if (ptr==mainw->do_not_free) return;
  free(ptr);
}

inline gint myround(gdouble n) {
  return (n>=0.)?(gint)(n + 0.5):(gint)(n - 0.5);
}

inline void 
clear_mainw_msg (void) {
  memset (mainw->msg,0,512);
}


inline int lives_10pow(int pow) {
  register int i,res=1;
  for (i=0;i<pow;i++) res*=10;
  return res;
}

inline int get_approx_ln(guint val) {
  int i;
  guint tval=1;

  for (i=0;i<16;i++) {
    if (tval>=val) return ++i;
    tval<<=1;
  }
  return i;
}



inline gchar *
g_strappend (gchar *string, gint len, const gchar *new) {
  gchar *tmp=g_strconcat (string,new,NULL);
  g_snprintf(string,len,"%s",tmp);
  g_free(tmp);
  return string;
}


#ifdef IS_IRIX
void setenv(const char *name, const char *val, int _xx) {
  int len  = strlen(name) + strlen(val) + 2;
  char *env = malloc(len);

  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    putenv(env);
  }
}
#endif

inline gdouble calc_time_from_frame (gint clip, gint frame) {
  return (frame-1.)/mainw->files[clip]->fps;
}



inline gint calc_frame_from_time (gint filenum, gdouble time) {
  // return the nearest frame for a given time
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.49999);
  return (frame<mainw->files[filenum]->frames)?frame:mainw->files[filenum]->frames;
}

inline gint calc_frame_from_time2 (gint filenum, gdouble time) {
  // return the nearest frame for a given time
  // allow max (frames+1)
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.49999);
  return (frame<mainw->files[filenum]->frames+1)?frame:mainw->files[filenum]->frames+1;
}

inline gint calc_frame_from_time3 (gint filenum, gdouble time) {
  // return the nearest frame for a given time
  // allow max (frames+1)
  int frame=0;
  if (time<0.) return mainw->files[filenum]->frames?1:0;
  frame=(gint)(time*mainw->files[filenum]->fps+1.);
  return (frame<mainw->files[filenum]->frames+1)?frame:mainw->files[filenum]->frames+1;
}


/* convert to/from a big endian 32 bit float for internal use */
inline float LEFloat_to_BEFloat(float f) {
  char *b=(char *)(&f);
  if (G_BYTE_ORDER==G_LITTLE_ENDIAN) {
    float fl;
    guchar rev[4];
    rev[0]=b[3];
    rev[1]=b[2];
    rev[2]=b[1];
    rev[3]=b[0];
    fl=*(float *)rev;
    return fl;
  }
  return f;
}


/////////////////////////////////////////////////////////////////////////////


void init_clipboard(void) {
  gint current_file=mainw->current_file;
  gchar *com;

  if (clipboard==NULL) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(0,"clipboard")) {
      mainw->error=TRUE;
      return;
    }
  }
  else if (clipboard->frames>0) { 
    // clear old clipboard
    // need to set current file to 0 before monitoring progress
    mainw->current_file=0;
    com=g_strdup_printf("smogrify delete_all %s",clipboard->handle);
    unlink(clipboard->info_file);
    dummyvar=system(com);
    cfile->progress_start=cfile->start;
    cfile->progress_end=cfile->end;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE,FALSE,_ ("Clearing the clipboard"));
    g_free(com);
  }

  mainw->current_file=current_file;
}



void d_print(const gchar *text) {
  GtkTextIter end_iter;
  GtkTextMark *mark;

  if (!capable->smog_version_correct) return;

  if (mainw->suppress_dprint) return;

  if (GTK_IS_TEXT_VIEW (mainw->textview1)) {
    gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1)),&end_iter);
    gtk_text_buffer_insert(gtk_text_view_get_buffer (GTK_TEXT_VIEW (mainw->textview1)),&end_iter,text,-1);
    if (mainw->current_file!=mainw->last_dprint_file&&mainw->current_file!=0&&mainw->multitrack==NULL&&(mainw->current_file==-1||cfile->clip_type!=CLIP_TYPE_GENERATOR)&&!mainw->no_switch_dprint) {
      gchar *switchtext,*tmp;
      if (mainw->current_file>0) {
	switchtext=g_strdup_printf (_ ("\n==============================\nSwitched to clip %s\n"),(tmp=g_path_get_basename(cfile->name)));
	g_free(tmp);
      }
      else {
	switchtext=g_strdup (_ ("\n==============================\nSwitched to empty clip\n"));
      }
      gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1)),&end_iter);
      gtk_text_buffer_insert(gtk_text_view_get_buffer (GTK_TEXT_VIEW (mainw->textview1)),&end_iter,switchtext,-1);
      g_free (switchtext);
    }
    if (mainw->current_file==-1||cfile->clip_type!=CLIP_TYPE_GENERATOR) mainw->last_dprint_file=mainw->current_file;
    gtk_text_buffer_get_end_iter(gtk_text_view_get_buffer(GTK_TEXT_VIEW(mainw->textview1)),&end_iter);
    mark=gtk_text_buffer_create_mark(gtk_text_view_get_buffer (GTK_TEXT_VIEW (mainw->textview1)),NULL,&end_iter,FALSE);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW (mainw->textview1),mark);
    gtk_text_buffer_delete_mark (gtk_text_view_get_buffer (GTK_TEXT_VIEW (mainw->textview1)),mark);
  }
}



gboolean add_lmap_error(int lerror, gchar *name, gpointer user_data, gint clipno, gint frameno, gdouble atime) {
  // potentially add a layout map error to the layout textbuffer
  GtkTextIter end_iter;
  gchar *text;
  gchar **array;
  GList *lmap;
  gdouble orig_fps;
  gint resampled_frame;
  gdouble max_time;

  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);


  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    text=g_strdup_printf(_("The set name has been changed from %s to %s. Affected layouts have been updated accordingly\n"),name,(gchar *)user_data);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_MISSING_CLIP:
    if (prefs->warning_mask&WARN_MASK_LAYOUT_MISSING_CLIPS) return FALSE;
    text=g_strdup_printf(_("The clip %s is missing from this set.\nIt is required by the following layouts:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
  case LMAP_ERROR_CLOSE_FILE:
    text=g_strdup_printf(_("The clip %s has been closed.\nIt is required by the following layouts:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
    text=g_strdup_printf(_("Frames have been shifted in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_DELETE_FRAMES:
    text=g_strdup_printf(_("Frames have been deleted from the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_DELETE_AUDIO:
    text=g_strdup_printf(_("Audio has been deleted from the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
    text=g_strdup_printf(_("Audio has been shifted in clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_ALTER_AUDIO:
    text=g_strdup_printf(_("Audio has been altered in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  case LMAP_ERROR_ALTER_FRAMES:
    text=g_strdup_printf(_("Frames have been altered in the clip %s.\nThe following layouts are affected:\n"),name);
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
    g_free(text);
    break;
  }


  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    lmap=mainw->current_layouts_map;
    while (lmap!=NULL) {
      array=g_strsplit(lmap->data,"|",-1);
      text=g_strdup_printf("%s\n",array[0]);
      gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      if (g_list_find(mainw->affected_layouts_map,array[0])==NULL) mainw->affected_layouts_map=g_list_append(mainw->affected_layouts_map,g_strdup(array[0]));
      g_free(text);
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_MISSING_CLIP:
  case LMAP_ERROR_CLOSE_FILE:
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit(lmap->data,"|",-1);
      text=g_strdup_printf("%s\n",array[0]);
      gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
      gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
      if (g_list_find(mainw->affected_layouts_map,array[0])==NULL) mainw->affected_layouts_map=g_list_append(mainw->affected_layouts_map,g_strdup(array[0]));
      g_free(text);
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
  case LMAP_ERROR_DELETE_FRAMES:
  case LMAP_ERROR_ALTER_FRAMES:
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit(lmap->data,"|",-1);
      orig_fps=strtod(array[3],NULL);
      resampled_frame=count_resampled_frames(frameno,orig_fps,mainw->files[clipno]->fps);
      if (resampled_frame<=atoi(array[2])) {
	text=g_strdup(array[0]);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
	if (g_list_find(mainw->affected_layouts_map,array[0])==NULL) mainw->affected_layouts_map=g_list_append(mainw->affected_layouts_map,g_strdup(array[0]));
	g_free(text);
      }
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
  case LMAP_ERROR_DELETE_AUDIO:
  case LMAP_ERROR_ALTER_AUDIO:
    lmap=(GList *)user_data;
    while (lmap!=NULL) {
      array=g_strsplit(lmap->data,"|",-1);
      max_time=strtod(array[4],NULL);
      if (max_time>0.&&atime<=max_time) {
	text=g_strdup(array[0]);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
	gtk_text_buffer_insert(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter,text,-1);
	if (g_list_find(mainw->affected_layouts_map,array[0])==NULL) mainw->affected_layouts_map=g_list_append(mainw->affected_layouts_map,g_strdup(array[0]));
	g_free(text);
      }
      g_strfreev(array);
      lmap=lmap->next;
    }
    break;
  }

  gtk_widget_set_sensitive (mainw->show_layout_errors, TRUE);
  return TRUE;
}


void clear_lmap_errors(void) {
  GtkTextIter start_iter,end_iter;
  GList *lmap;

  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&start_iter);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&end_iter);
  gtk_text_buffer_delete(GTK_TEXT_BUFFER(mainw->layout_textbuffer),&start_iter,&end_iter);

  lmap=mainw->affected_layouts_map;

  while (lmap!=NULL) {
    g_free(lmap->data);
    lmap=lmap->next;
  }
  g_list_free(lmap);

  mainw->affected_layouts_map=NULL;
  gtk_widget_set_sensitive (mainw->show_layout_errors, FALSE);

}


gboolean check_for_lock_file(gchar *set_name, gint type) {
  // check for lock file
  int info_fd;
  gchar *msg=NULL;
  size_t bytes;

  gchar *info_file=g_strdup_printf("%s/.locks.%d",prefs->tmpdir,getpid());
  gchar *com=g_strdup_printf("smogrify check_for_lock \"%s\" %s %d >%s",set_name,capable->myname,getpid(),info_file);

  int lockpid;

  unlink(info_file);
  pthread_mutex_lock(&mainw->gtk_mutex);
  dummyvar=system(com);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  g_free(com);

  clear_mainw_msg();
  
  info_fd=open(info_file,O_RDONLY);
  if (info_fd>-1) {
    if ((bytes=read(info_fd,mainw->msg,256))>0) {
      close(info_fd);
      memset(mainw->msg+bytes,0,1);
      lockpid=atoi(mainw->msg);

      if (type==0) {
	msg=g_strdup_printf(_("Set %s\ncannot be opened, as it is in use\nby another copy of LiVES.\n"),set_name);
	pthread_mutex_lock(&mainw->gtk_mutex);
	do_error_dialog(msg);
	pthread_mutex_unlock(&mainw->gtk_mutex);
      }
      else if (type==1) {
	msg=g_strdup_printf(_("\nThe set %s is currently in use by another copy of LiVES.\nPlease choose another set name.\n"),set_name);
	do_blocking_error_dialog(msg);
      }
      if (msg!=NULL) {
	g_free(msg);
      }
      unlink(info_file);
      g_free(info_file);
      return FALSE;
    }
  }
  close (info_fd);
  unlink(info_file);
  g_free(info_file);

  return TRUE;
}


gboolean is_legal_set_name(gchar *set_name, gboolean allow_dupes) {
  gchar *msg;
  gchar *reject=" /\\*\"";

  if (!strlen(set_name)) {
    do_blocking_error_dialog(_("\nSet names may not be blank.\n"));
    return FALSE;
  }
  if (strcspn(set_name,reject)!=strlen(set_name)) {
    msg=g_strdup_printf(_("\nSet names may not contain spaces or the characters%s.\n"),reject);
    do_blocking_error_dialog(msg);
    g_free(msg);
    return FALSE;
  }

  if (!check_for_lock_file(set_name,1)) return FALSE;
  
  if (!allow_dupes) {
    gchar *set_dir=g_strdup_printf("%s/%s",prefs->tmpdir,set_name);
    if (g_file_test(set_dir,G_FILE_TEST_IS_DIR)) {
      g_free(set_dir);
      msg=g_strdup_printf(_("\nThe set %s already exists.\nPlease choose another set name.\n"),set_name);
      do_blocking_error_dialog(msg);
      g_free(msg);
      return FALSE;
    }
    g_free(set_dir);
  }

  return TRUE;
}


gboolean check_frame_count(gint idx) {
  // check number of frames is correct
  gchar *frame=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,mainw->files[idx]->handle,mainw->files[idx]->frames,prefs->image_ext);

  if (!g_file_test(frame,G_FILE_TEST_EXISTS)) {
    // not enough frames
    g_free(frame);
    return FALSE;
  }
  g_free(frame);

  frame=g_strdup_printf("%s/%s/%08d.%s",prefs->tmpdir,mainw->files[idx]->handle,mainw->files[idx]->frames+1,prefs->image_ext);
  if (g_file_test(frame,G_FILE_TEST_EXISTS)) {
    // too many frames
    g_free(frame);
    return FALSE;
  }
  g_free(frame);

  return TRUE;
}



void get_frame_count(gint idx) {
  // sets mainw->files[idx]->frames with current framecount
  gint info_fd;
  size_t bytes;
  gchar *info_file=g_strdup_printf("%s/.check.%d",prefs->tmpdir,getpid());
  gchar *com=g_strdup_printf("smogrify count_frames %s >%s",mainw->files[idx]->handle,info_file);
  dummyvar=system(com);
  g_free(com);
  
  info_fd=open(info_file,O_RDONLY);
  if ((bytes=read(info_fd,mainw->msg,256))>0) {
    memset(mainw->msg+bytes,0,1);
    mainw->files[idx]->frames=atoi(mainw->msg);
  }
  close(info_fd);
  unlink(info_file);
  g_free(info_file);
}






void
get_next_free_file(void) {
  // get next free file slot, or -1 if we are full
  while ((mainw->first_free_file!=-1)&&mainw->files[mainw->first_free_file]!=NULL) {
    mainw->first_free_file++;
    if (mainw->first_free_file>=MAX_FILES) mainw->first_free_file=-1;
  }
}


void
get_dirname(gchar *filename) {
  gchar *tmp;
  // get directory name from a file
  //filename should point to gchar[256]
  g_snprintf (filename,256,"%s%s",(tmp=g_path_get_dirname (filename)),G_DIR_SEPARATOR_S);
  g_free(tmp);

  if (!strcmp(filename,"//")) {
    memset(filename+1,0,1);
    return;
  }

  if (!strncmp(filename,"./",2)) {
    gchar *tmp1=g_get_current_dir(),*tmp=g_strdup_printf("%s/%s",tmp1,filename+2);
    g_free(tmp1);
    g_snprintf(filename,256,"%s",tmp);
    g_free(tmp);
  }
}

void
get_basename(gchar *filename) {
  // get basename from a file
  //filename should point to gchar[256]
  gchar *tmp=g_path_get_basename(filename);
  g_snprintf (filename,256,"%s",tmp);
  g_free(tmp);
}

void
get_filename(gchar *filename) {
  // get filename (part without extension) of a file
  //filename should point to gchar[256]
  gchar **array;
  get_basename(filename);
  array=g_strsplit(filename,".",-1);
  g_snprintf(filename,256,"%s",array[0]);
  g_strfreev(array);
}


gchar *get_extension(const gchar *filename) {
  gchar *tmp=g_path_get_basename(filename);
  gint ntok=get_token_count((gchar *)filename,'.');
  gchar **array=g_strsplit(tmp,".",-1);
  gchar *ret=g_strdup(array[ntok-1]);
  g_strfreev(array);
  g_free(tmp);
  return ret;
}





gchar *ensure_extension(gchar *fname, gchar *ext) {
  gchar *ret;
  if (!strcmp(fname+strlen(fname)-strlen(ext),ext)) return fname;
  ret=g_strconcat(fname,ext,NULL);
  g_free(fname);
  return ret;
}


gboolean ensure_isdir(gchar *fname) {
  // ensure dirname ends in a single dir separator
  // fname should be gchar[256]

  // returns TRUE if fname was altered

  size_t slen=strlen(fname);
  size_t offs=slen-1;
  gchar *tmp;

  while (offs>=0&&!strcmp(fname+offs,G_DIR_SEPARATOR_S)) offs--;
  if (offs==slen-2) return FALSE;
  memset(fname+offs+1,0,1);
  tmp=g_strdup_printf("%s%s",fname,G_DIR_SEPARATOR_S);
  g_snprintf(fname,256,"%s",tmp);
  g_free(tmp);
  return TRUE;
}


void
get_location(const gchar *exe, gchar *val, gint maxlen) {
  gchar *loc;
  if ((loc=g_find_program_in_path (exe))!=NULL) {
    g_snprintf (val,maxlen,"%s",loc);
    g_free (loc);
  }
  else {
    memset (val,0,1);
  }
}


gchar *repl_tmpdir(gchar *entry, gboolean fwd) {
  // replace prefs->tmpdir with string tmpdir or vice-versa. This allows us to relocate tmpdir if necessary.
  // used for layout.map file
  // return value should be g_free()'d
  gchar *string=g_strdup(entry);;

  if (fwd) {
    if (!strncmp(entry,prefs->tmpdir,strlen(prefs->tmpdir))) {
      g_free(string);
      string=g_strdup_printf("tmpdir%s",entry+strlen(prefs->tmpdir));
    }
  }
  else {
    if (!strncmp(entry,"tmpdir",6)) {
      g_free(string);
      string=g_strdup_printf("%s%s",prefs->tmpdir,entry+6);
    }
  }
  return string;
}


void remove_layout_files(GList *map) {
  gchar *com,*msg;
  gchar *fname,*fdir;
  GList *lmap,*lmap_next,*cmap,*cmap_next;
  size_t maplen;
  int i;

  while (map!=NULL) {
    if (map->data!=NULL) {
      maplen=strlen(map->data);

      // remove from mainw->current_layouts_map
      cmap=mainw->current_layouts_map;
      while (cmap!=NULL) {
	cmap_next=cmap->next;
	if (!strcmp((gchar *)cmap->data,(gchar *)map->data)) {
	  mainw->current_layouts_map=g_list_remove_link(mainw->current_layouts_map,cmap);
	  break;
	}
	cmap=cmap_next;
      }

      fname=repl_tmpdir(map->data,FALSE);
      msg=g_strdup_printf(_("Removing layout %s\n"),fname);
      d_print(msg);
      g_free(msg);
      
      com=g_strdup_printf("/bin/rm \"%s\" 2>/dev/null",fname);
      dummyvar=system(com);
      g_free(com);
      
      if (!strncmp(fname,prefs->tmpdir,strlen(prefs->tmpdir))) {
	// is in tmpdir, safe to remove parents
	com=g_strdup_printf("touch %s/noremove >/dev/null 2>&1",prefs->tmpdir);
	dummyvar=system(com);
	g_free(com);
	
	fdir=g_path_get_dirname (fname);
	com=g_strdup_printf("/bin/rmdir -p \"%s\" 2>/dev/null",fdir);
	dummyvar=system(com);
	g_free(com);
	g_free(fdir);
	
	com=g_strdup_printf("/bin/rm %s/noremove 2>/dev/null",prefs->tmpdir);
	dummyvar=system(com);
	g_free(com);
      }

      // remove from mainw->files[]->layout_map
      for (i=1;i<=MAX_FILES;i++) {
	if (mainw->files[i]!=NULL) {
	  if (mainw->files[i]->layout_map!=NULL) {
	    lmap=mainw->files[i]->layout_map;
	    while (lmap!=NULL) {
	      lmap_next=lmap->next;
	      if (!strncmp(lmap->data,map->data,maplen)) {
		// remove matching entry
		if (lmap->prev!=NULL) lmap->prev->next=lmap_next;
		else mainw->files[i]->layout_map=lmap_next;
		if (lmap->next!=NULL) lmap_next->prev=lmap->prev;
		lmap->next=lmap->prev=NULL;
		g_free(lmap->data);
		g_list_free(lmap);
	      }
	      lmap=lmap_next;
	    }
	  }
	}
      }
    }
    map=map->next;
  }

  // save updated layout.map
  save_layout_map(NULL,NULL,NULL,NULL);
  
}



void
get_play_times(void) {
  // update the on-screen timer bars,
  // and if we are not playing,
  // get play times for video, audio channels, and total (longest) time
  gchar *tmpstr;
  gdouble offset=0;
  gdouble offset_left=0;
  gdouble offset_right=0;
  gdouble allocwidth;
  gdouble allocheight;

  if (mainw->current_file==-1||mainw->foreign||cfile==NULL||mainw->multitrack!=NULL) return;

  if (mainw->playing_file==-1) {
    get_total_time (cfile);
    //set_main_title (cfile->name,0);
  }

  if (!mainw->is_ready) return;

  // draw timer bars
  allocwidth=mainw->video_draw->allocation.width;
  allocheight=mainw->video_draw->allocation.height;

  
  if (mainw->laudio_drawable!=NULL) {
    gdk_draw_rectangle (mainw->laudio_drawable,
			mainw->laudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->laudio_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }
  
  
  if (mainw->raudio_drawable!=NULL) {
    gdk_draw_rectangle (mainw->raudio_drawable,
			mainw->raudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->raudio_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }

  if (mainw->video_drawable!=NULL) {
    gdk_draw_rectangle (mainw->video_drawable,
			mainw->video_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->video_draw)],
			TRUE,
			0, 0,
			allocwidth,
			allocheight
			);
  }

  if (cfile->frames>0) {
    offset_left=(cfile->start-1)/cfile->fps/cfile->total_time*allocwidth;
    offset_right=(cfile->end)/cfile->fps/cfile->total_time*allocwidth;

    
    if (mainw->video_drawable!=NULL) {
      gdk_draw_rectangle (mainw->video_drawable,
			  mainw->video_draw->style->black_gc,
			  TRUE,
			  0, 0,
			  cfile->video_time/cfile->total_time*allocwidth-1,
			  prefs->bar_height);
      
      gdk_draw_rectangle (mainw->video_drawable,
			  mainw->video_draw->style->white_gc,
			  TRUE,
			  offset_left, 0,
			  offset_right-offset_left,
			  prefs->bar_height);
    }
  }
  if (cfile->achans>0) {
    if (mainw->playing_file>-1) {
      offset_left=((mainw->playing_sel&&(prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE))?cfile->start-1.:mainw->audio_start-1.)/cfile->fps/cfile->total_time*allocwidth;
      if (mainw->audio_end) {
	offset_right=(((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE))?cfile->end:mainw->audio_end)/cfile->fps/cfile->total_time*allocwidth;
      }
      else {
	offset_right=allocwidth*cfile->laudio_time/cfile->total_time;
      }
    }
    
    
    if (mainw->laudio_drawable!=NULL) {
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->black_gc,
			  TRUE,
			  0, 0,
			  cfile->laudio_time/cfile->total_time*allocwidth-1,
			  prefs->bar_height);
    
    
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->white_gc,
			  TRUE,
			  offset_left, 0,
			  offset_right-offset_left,
			  prefs->bar_height);
  
    
      gdk_draw_rectangle (mainw->laudio_drawable,
			  mainw->laudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->laudio_draw)],
			  TRUE,
			  cfile->laudio_time/cfile->total_time*allocwidth, 0,
			  allocwidth-(cfile->laudio_time/cfile->total_time*allocwidth),
			  prefs->bar_height);
    }
    if (cfile->achans>1) {
      if (mainw->raudio_drawable!=NULL) {
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->black_gc,
			    TRUE,
			    0, 0,
			    cfile->raudio_time/cfile->total_time*allocwidth-1,
			    prefs->bar_height);
      
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->white_gc,
			    TRUE,
			    offset_left, 0,
			    offset_right-offset_left,
			    prefs->bar_height);
      
      
	gdk_draw_rectangle (mainw->raudio_drawable,
			    mainw->raudio_draw->style->bg_gc[GTK_WIDGET_STATE (mainw->raudio_draw)],
			    TRUE,
			    cfile->raudio_time/cfile->total_time*allocwidth, 0,
			    allocwidth-(cfile->raudio_time/cfile->total_time*allocwidth),
			    prefs->bar_height);
      }
    }
  }

  // playback cursors
  if (mainw->playing_file>-1) {
    if (cfile->frames>0) {
      offset=(mainw->actual_frame-.5)/cfile->fps;
      offset/=cfile->total_time/allocwidth;
      if (mainw->video_drawable!=NULL) {
	if (offset>=offset_left&&offset<=offset_right) {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
      
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->video_drawable,
			 mainw->video_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
      }
    }
    if (cfile->achans>0&&cfile->is_loaded) {
      if ((prefs->audio_player==AUD_PLAYER_JACK||prefs->audio_player==AUD_PLAYER_PULSE)&&(mainw->event_list==NULL||!mainw->preview)) {
#ifdef ENABLE_JACK
	if (mainw->jackd!=NULL&&prefs->audio_player==AUD_PLAYER_JACK) {
	  offset=allocwidth*((gdouble)mainw->jackd->seek_pos/cfile->arate/cfile->achans/cfile->asampsize*8)/cfile->total_time;
	}
#endif
#ifdef HAVE_PULSE_AUDIO
	if (mainw->pulsed!=NULL&&prefs->audio_player==AUD_PLAYER_PULSE) {
	  offset=allocwidth*((gdouble)mainw->pulsed->seek_pos/cfile->arate/cfile->achans/cfile->asampsize*8)/cfile->total_time;
	}
#endif
      }
      else offset=allocwidth*(mainw->aframeno-.5)/cfile->fps/cfile->total_time;
      if (mainw->laudio_drawable!=NULL) {
	if ((offset>=offset_left&&offset<=offset_right)||mainw->loop) {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
      
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->laudio_drawable,
			 mainw->laudio_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight-prefs->bar_height);
	}
      }

      if (cfile->achans>1) {
	if (mainw->raudio_drawable!=NULL) {
	  if ((offset>=offset_left&&offset<=offset_right)||mainw->loop) {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->black_gc,
			   offset, 0,
			   offset,
			   prefs->bar_height);
	  }
	  else {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->white_gc,
			   offset, 0,
			   offset,
			   prefs->bar_height);
	  }
	  
	  
	  if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->black_gc,
			   offset, prefs->bar_height,
			   offset,
			   allocheight-prefs->bar_height);
	  }
	  else {
	    gdk_draw_line (mainw->raudio_drawable,
			   mainw->raudio_draw->style->white_gc,
			   offset, prefs->bar_height,
			   offset,
			   allocheight-prefs->bar_height);
	  }
	}
      }
    }
    GTK_RULER (mainw->hruler)->position=offset*cfile->total_time/allocwidth;
    gtk_widget_queue_draw (mainw->hruler);
  }
  
  if (mainw->playing_file==-1||(mainw->switch_during_pb&&!mainw->faded)) {
    if (cfile->total_time>0.) {
      // set the range of the timeline
      if (!cfile->opening_loc) {
	gtk_widget_show (mainw->hruler);
      }
      gtk_widget_show (mainw->eventbox5);
      gtk_widget_show (mainw->video_draw);
      gtk_widget_show (mainw->laudio_draw);
      gtk_widget_show (mainw->raudio_draw);
      GTK_RULER (mainw->hruler)->upper=cfile->total_time;
      draw_little_bars();
      if (mainw->playing_file==-1&&prefs->sepwin_type==1&&mainw->sep_win&&cfile->is_loaded) {
	if (mainw->preview_box==NULL) {
	  // create the preview box that shows frames
	  make_preview_box();
	}
	// and add it the play window
	if (mainw->preview_box->parent==NULL&&cfile->clip_type==CLIP_TYPE_DISK&&!mainw->is_rendering) {
	  gtk_widget_queue_draw(mainw->play_window);
	  gtk_container_add (GTK_CONTAINER (mainw->play_window), mainw->preview_box);
	}
	load_preview_image(FALSE);
      }
    }
    else {
      gtk_widget_hide (mainw->hruler);
      gtk_widget_hide (mainw->eventbox5);
    }

    if (cfile->opening_loc||(cfile->frames==123456789&&cfile->opening)) {
      tmpstr=g_strdup(_ ("Video [opening...]"));
    }
    else {
      if (cfile->video_time>0.) {
	tmpstr=g_strdup_printf(_ ("Video [%.2f sec]"),cfile->video_time);
      }
      else {
	if (cfile->video_time<=0.&&cfile->frames>0) {
	  tmpstr=g_strdup (_ ("(Undefined)"));
	}
	else {
	  tmpstr=g_strdup (_ ("(No video)"));
	}
      }
    }
    gtk_label_set_text(GTK_LABEL(mainw->vidbar),tmpstr);
    g_free(tmpstr);
    if (cfile->achans==0) {
      tmpstr=g_strdup (_ ("(No audio)"));
    }
    else {
      if (cfile->opening_audio) {
	if (cfile->achans==1) {
	  tmpstr=g_strdup (_ ("Mono  [opening...]"));
	}
	else {
	  tmpstr=g_strdup (_ ("Left Audio [opening...]"));
	}
      }
      else {
	if (cfile->achans==1) {
	  tmpstr=g_strdup_printf(_ ("Mono [%.2f sec]"),cfile->laudio_time);
	}
	else {
	  tmpstr=g_strdup_printf(_ ("Left Audio [%.2f sec]"),cfile->laudio_time);
	}
      }
    }
    gtk_label_set_text(GTK_LABEL(mainw->laudbar),tmpstr);
    g_free(tmpstr);
    if (cfile->achans>1) {
      if (cfile->opening_audio) {
	tmpstr=g_strdup (_ ("Right Audio [opening...]"));
      }
      else {
	tmpstr=g_strdup_printf(_ ("Right Audio [%.2f sec]"),cfile->raudio_time);
      }
      gtk_label_set_text(GTK_LABEL(mainw->raudbar),tmpstr);
      gtk_widget_show (mainw->raudbar);
      g_free(tmpstr);
    }
    else {
      gtk_widget_hide (mainw->raudbar);
    }
  }
  gtk_widget_queue_draw(mainw->video_draw);
  gtk_widget_queue_draw(mainw->laudio_draw);
  gtk_widget_queue_draw(mainw->raudio_draw);
  gtk_widget_queue_draw(mainw->vidbar);
  gtk_widget_queue_draw(mainw->hruler);
}
    

void 
draw_little_bars (void) {
  //draw the vertical bars when we are not playing
  gdouble allocheight=mainw->video_draw->allocation.height-prefs->bar_height;
  gdouble offset=cfile->pointer_time/cfile->total_time*mainw->vidbar->allocation.width;
  gint frame;

  if (pthread_mutex_trylock(&mainw->gtk_mutex)) return;

  if (!(frame=calc_frame_from_time(mainw->current_file,cfile->pointer_time))) frame=cfile->frames;

  if (cfile->frames>0) {
    if (frame>=cfile->start&&frame<=cfile->end) {
      if (mainw->video_drawable!=NULL) {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->black_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      else {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->white_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      
      if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->black_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
      else {
	gdk_draw_line (mainw->video_drawable,
		       mainw->video_draw->style->white_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
    }
  }
  if (cfile->achans>0) {
    if (mainw->laudio_drawable!=NULL) {
      if ((frame>=cfile->start&&frame<=cfile->end)||mainw->loop) {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->black_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      else {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->white_gc,
		       offset, 0,
		       offset,
		       prefs->bar_height);
      }
      
      if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->black_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
      else {
	gdk_draw_line (mainw->laudio_drawable,
		       mainw->laudio_draw->style->white_gc,
		       offset, prefs->bar_height,
		       offset,
		       allocheight);
      }
    }
    if (cfile->achans>1) {
      if (mainw->raudio_drawable!=NULL) {
	if ((frame>=cfile->start&&frame<=cfile->end)||mainw->loop) {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->black_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	else {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->white_gc,
			 offset, 0,
			 offset,
			 prefs->bar_height);
	}
	
	
	if (palette->style&STYLE_3||palette->style==STYLE_PLAIN) { // light style
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->black_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight);
	}
	else {
	  gdk_draw_line (mainw->raudio_drawable,
			 mainw->raudio_draw->style->white_gc,
			 offset, prefs->bar_height,
			 offset,
			 allocheight);
	}
      }
    }
  }
  pthread_mutex_unlock(&mainw->gtk_mutex);
}




void get_total_time (file *file) {
  // get times (video, left and right audio)

  file->laudio_time=file->raudio_time=file->video_time=file->total_time=0.;

  if (file->opening&&file->frames!=123456789) {
    if (file->frames*file->fps>0) {
      file->video_time=file->total_time=file->frames/file->fps;
    }
    return;
  }

  if (file->fps>0.) {
    file->total_time=file->video_time=file->frames/file->fps;
  }


  if (file->asampsize*file->arate*file->achans) {
    file->laudio_time=file->afilesize/file->asampsize*8./file->arate/file->achans;
    if (file->achans>1) {
      file->raudio_time=file->laudio_time;
    }
  }

  if (file->laudio_time>file->total_time) file->total_time=file->laudio_time;
  if (file->raudio_time>file->total_time) file->total_time=file->raudio_time;

  if (file->laudio_time+file->raudio_time==0.&&!file->opening) {
    file->achans=file->afilesize=file->asampsize=file->arate=file->arps=0;
  }
}



void 
find_when_to_stop (void) {
  // work out when to stop playing
  //
  // ---------------
  //        no loop              loop to fit                 loop cont
  //        -------              -----------                 ---------
  // a>v    stop on video end    stop on audio end           no stop
  // v>a    stop on video end    stop on video end           no stop
  // generator start - not playing : stop on vid_end, unless pure audio;
  if (cfile->clip_type==CLIP_TYPE_GENERATOR) mainw->whentostop=STOP_ON_VID_END;
  else if (mainw->multitrack!=NULL) mainw->whentostop=STOP_ON_VID_END;
  else if (cfile->clip_type!=CLIP_TYPE_DISK&&cfile->clip_type!=CLIP_TYPE_FILE) mainw->whentostop=NEVER_STOP;
  else if (cfile->opening_only_audio) mainw->whentostop=STOP_ON_AUD_END;
  else if (cfile->opening_audio) mainw->whentostop=STOP_ON_VID_END;
  else if (mainw->loop_cont&&!mainw->preview) mainw->whentostop=NEVER_STOP;
  else if (mainw->loop&&cfile->achans>0&&!mainw->is_rendering&&(mainw->audio_end/cfile->fps)<MAX (cfile->laudio_time,cfile->raudio_time)) mainw->whentostop=STOP_ON_AUD_END;
  else mainw->whentostop=STOP_ON_VID_END; // tada...
}

void 
minimise_aspect_delta (gdouble aspect,gint hblock,gint vblock,gint hsize,gint vsize,gint *width,gint *height) {
  // we will use trigonometry to calculate the smallest difference between a given
  // aspect ratio and the actual frame size. If the delta is smaller than current 
  // we set the height and width
  gint cw=width[0];
  gint ch=height[0];

  gint real_width,real_height;
  gint delta,current_delta=(hsize-cw)*(hsize-cw)+(vsize-ch)*(vsize-ch);

  // minimise d[(x-x1)^2 + (y-y1)^2]/d[x1], to get approximate values
  gint calc_width=(gint)((vsize+aspect*hsize)*aspect/(aspect*aspect+1.));

  int i;

#ifdef DEBUG_ASPECT
  g_printerr ("aspect %.8f : width %d height %d is best fit\n",aspect,calc_width,(gint)(calc_width/aspect));
#endif
  // use the block size to find the nearest allowed size
  for (i=-1;i<2;i++) {
    real_width=(gint)(calc_width/hblock+i)*hblock;
    real_height=(gint)(real_width/aspect/vblock+.5)*vblock;
    delta=(hsize-real_width)*(hsize-real_width)+(vsize-real_height)*(vsize-real_height);
#ifdef DEBUG_ASPECT
    g_printerr ("block quantise to %d x %d\n",real_width,real_height);
#endif
    if (delta<current_delta) {
#ifdef DEBUG_ASPECT
      g_printerr ("is better fit\n");
#endif
      current_delta=delta;
      width[0]=real_width;
      height[0]=real_height;
    }
  }
}

void 
zero_spinbuttons (void) {
  g_signal_handler_block(mainw->spinbutton_start,mainw->spin_start_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_start),0.,0.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_start),0.);
  g_signal_handler_unblock(mainw->spinbutton_start,mainw->spin_start_func);
  g_signal_handler_block(mainw->spinbutton_end,mainw->spin_end_func);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(mainw->spinbutton_end),0.,0.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(mainw->spinbutton_end),0.);
  g_signal_handler_unblock(mainw->spinbutton_end,mainw->spin_end_func);
}


void 
hide_cursor(GdkWindow *window) {
  char cursor_bits[] = {0x00};
  char cursormask_bits[] = {0x00};
  GdkPixmap *source, *mask;
  GdkColor fg = { 0, 0, 0, 0 };
  GdkColor bg = { 0, 0, 0, 0 };

  if (hidden_cursor==NULL) {
    //make the cursor invisible in playback windows
    source = gdk_bitmap_create_from_data (NULL, cursor_bits,
					  1, 1);
    mask = gdk_bitmap_create_from_data (NULL, cursormask_bits,
					1, 1);
    hidden_cursor = gdk_cursor_new_from_pixmap (source, mask, &fg, &bg, 0, 0);
    gdk_pixmap_unref (source);
    gdk_pixmap_unref (mask);
  }

  gdk_window_set_cursor (window, hidden_cursor);
}


void 
unhide_cursor(GdkWindow *window) {
  gdk_window_set_cursor(window,NULL);
}



inline void toggle_button_toggle (GtkToggleButton *tbutton) {
  if (gtk_toggle_button_get_active(tbutton)) gtk_toggle_button_set_active(tbutton,FALSE);
  else gtk_toggle_button_set_active(tbutton,FALSE);
}




gboolean switch_aud_to_jack(void) {
#ifdef ENABLE_JACK
  if (mainw->is_ready) {
    if (prefs->jack_opts&JACK_OPTS_START_ASERVER) lives_jack_init();
    if (mainw->jackd==NULL) {
      jack_audio_init();
      jack_audio_read_init();
      mainw->jackd=jack_get_driver(0,TRUE);
      if (jack_open_device(mainw->jackd)) {
	mainw->jackd=NULL;
	return FALSE;
      }
      mainw->jackd->whentostop=&mainw->whentostop;
      mainw->jackd->cancelled=&mainw->cancelled;
      mainw->jackd->in_use=FALSE;
      mainw->jackd->play_when_stopped=(prefs->jack_opts&JACK_OPTS_NOPLAY_WHEN_PAUSED)?FALSE:TRUE;
      jack_driver_activate(mainw->jackd);
    }
    gtk_widget_show(mainw->vol_toolitem);
    gtk_widget_show(mainw->vol_label);
    gtk_widget_show (mainw->recaudio_submenu);
  }

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read!=NULL) {
    pulse_close_client(mainw->pulsed_read,FALSE);
    mainw->pulsed_read=NULL;
  }

  if (mainw->pulsed!=NULL) {
    pulse_close_client(mainw->pulsed,TRUE);
    mainw->pulsed=NULL;
  }
#endif

  prefs->audio_player=AUD_PLAYER_JACK;
  set_pref("audio_player","jack");
  return TRUE;

#endif
  return FALSE;
}



gboolean switch_aud_to_pulse(void) {
#ifdef HAVE_PULSE_AUDIO
  if (mainw->is_ready) {
    lives_pulse_init();
    if (mainw->pulsed==NULL) {
      pulse_audio_init();
      //pulse_audio_read_init();
      mainw->pulsed=pulse_get_driver(TRUE);
      mainw->pulsed->whentostop=&mainw->whentostop;
      mainw->pulsed->cancelled=&mainw->cancelled;
      mainw->pulsed->in_use=FALSE;
      pulse_driver_activate(mainw->pulsed);
    }
    gtk_widget_show(mainw->vol_toolitem);
    gtk_widget_show(mainw->vol_label);
    gtk_widget_show (mainw->recaudio_submenu);
  }

  if (mainw->jackd_read!=NULL) {
    jack_close_device(mainw->jackd_read,TRUE);
    mainw->jackd_read=NULL;
  }

#ifdef ENABLE_JACK
  if (mainw->jackd!=NULL) {
    jack_close_device(mainw->jackd,TRUE);
    mainw->jackd=NULL;
  }

  prefs->audio_player=AUD_PLAYER_PULSE;
  set_pref("audio_player","pulse");
  return TRUE;
#endif

#endif
  return FALSE;
}



void switch_aud_to_sox(void) {
  prefs->audio_player=AUD_PLAYER_SOX;
  get_pref_default("sox_command",prefs->audio_play_command,256);
  set_pref("audio_player","sox");
  set_pref("audio_play_command",prefs->audio_play_command);
  if (mainw->is_ready) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
    gtk_widget_hide (mainw->recaudio_submenu);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read!=NULL) {
    jack_close_device(mainw->jackd_read,TRUE);
    mainw->jackd_read=NULL;
  }

  if (mainw->jackd!=NULL) {
    jack_close_device(mainw->jackd,TRUE);
    mainw->jackd=NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read!=NULL) {
    pulse_close_client(mainw->pulsed_read,FALSE);
    mainw->pulsed_read=NULL;
  }

  if (mainw->pulsed!=NULL) {
    pulse_close_client(mainw->pulsed,TRUE);
    mainw->pulsed=NULL;
  }
#endif

}



void
switch_aud_to_mplayer(void) {
  int i;
  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL) {
      if (i!=mainw->current_file&&mainw->files[i]->opening) {
	do_error_dialog(_ ("LiVES cannot switch to mplayer whilst clips are loading."));
	return;
      }
    }
  }
  
  prefs->audio_player=AUD_PLAYER_MPLAYER;
  get_pref_default("mplayer_audio_command",prefs->audio_play_command,256);
  set_pref("audio_player","mplayer");
  set_pref("audio_play_command",prefs->audio_play_command);
  if (mainw->is_ready) {
    gtk_widget_hide(mainw->vol_toolitem);
    gtk_widget_hide(mainw->vol_label);
    gtk_widget_hide (mainw->recaudio_submenu);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read!=NULL) {
    jack_close_device(mainw->jackd_read,TRUE);
    mainw->jackd_read=NULL;
  }

  if (mainw->jackd!=NULL) {
    jack_close_device(mainw->jackd,TRUE);
    mainw->jackd=NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read!=NULL) {
    pulse_close_client(mainw->pulsed_read,FALSE);
    mainw->pulsed_read=NULL;
  }

  if (mainw->pulsed!=NULL) {
    pulse_close_client(mainw->pulsed,TRUE);
    mainw->pulsed=NULL;
  }
#endif

}



void
prepare_to_play_foreign(void) {
  // here we are going to 'play' a captured external window
  gint new_file=mainw->first_free_file;

  // create a new 'file' to play into
  if (!get_new_handle(new_file,NULL)) {
    return;
  }

  mainw->current_file=new_file;

  if (mainw->rec_achans>0) {
    cfile->arate=cfile->arps=mainw->rec_arate;
    cfile->achans=mainw->rec_achans;
    cfile->asampsize=mainw->rec_asamps;
    cfile->signed_endian=mainw->rec_signed_endian;
#ifdef HAVE_PULSE_AUDIO
    if (mainw->rec_achans>0&&prefs->audio_player==AUD_PLAYER_PULSE&&mainw->pulsed_read==NULL) pulse_rec_audio_to_clip(mainw->current_file,TRUE);
#endif
#ifdef ENABLE_JACK
    if (mainw->rec_achans>0&&prefs->audio_player==AUD_PLAYER_JACK&&mainw->jackd_read==NULL) jack_rec_audio_to_clip(mainw->current_file,TRUE);
#endif
  }

  cfile->hsize=mainw->foreign_width/2+1;
  cfile->vsize=mainw->foreign_height/2+3;

  cfile->fps=cfile->pb_fps=mainw->rec_fps;

  resize(-2);

  gtk_widget_show (mainw->playframe);
  gtk_widget_show (mainw->playarea);
  while (g_main_context_iteration(NULL,FALSE));

  // size must be exact, must not be larger than play window or we end up with nothing
  mainw->pwidth=mainw->playframe->allocation.width-H_RESIZE_ADJUST+2;
  mainw->pheight=mainw->playframe->allocation.height-V_RESIZE_ADJUST+2;

  cfile->hsize=mainw->pwidth;
  cfile->vsize=mainw->pheight;

  // for some strange reason we now have to do this twice...and now it doesn't
  // always work
  gtk_socket_add_id(GTK_SOCKET(mainw->playarea),mainw->foreign_id);
  gtk_socket_add_id(GTK_SOCKET(mainw->playarea),mainw->foreign_id);
  while (g_main_context_iteration(NULL,FALSE));

  mainw->foreign_map=gdk_pixmap_foreign_new(mainw->foreign_id);
  mainw->foreign_cmap=gdk_colormap_get_system();

  mainw->play_start=1;
  if (mainw->rec_vid_frames==-1) mainw->play_end=INT_MAX;
  else mainw->play_end=mainw->rec_vid_frames;

  mainw->rec_samples=-1;

  omute=mainw->mute;
  osepwin=mainw->sep_win;
  ofs=mainw->fs;
  ofaded=mainw->faded;
  odouble=mainw->double_size;

  mainw->mute=TRUE;
  mainw->sep_win=FALSE;
  mainw->fs=FALSE;
  mainw->faded=TRUE;
  mainw->double_size=FALSE;

  gtk_widget_hide(mainw->t_double);
  gtk_widget_hide(mainw->t_bckground);
  gtk_widget_hide(mainw->t_sepwin);
  gtk_widget_hide(mainw->t_infobutton);
}


gboolean
after_foreign_play(void) {
  // read details from capture file
  int capture_fd;
  gchar *capfile=g_strdup_printf("%s/.capture.%d",prefs->tmpdir,getpid());
  gchar capbuf[256];
  ssize_t length;
  gint new_file;
  gint new_frames=0;
  gint old_file=mainw->current_file;

  gchar *com;
  gchar **array;
  gchar file_name[256];

  // assume for now we only get one clip passed back
  if ((capture_fd=open(capfile,O_RDONLY))>0) {
    memset(capbuf,0,256);
    if ((length=read(capture_fd,capbuf,256))) {
      if (get_token_count (capbuf,'|')>2) {
	array=g_strsplit(capbuf,"|",3);
	new_frames=atoi(array[1]);
	new_file=mainw->first_free_file;
	mainw->current_file=new_file;
	cfile=(file *)(g_malloc(sizeof(file)));
	g_snprintf(cfile->handle,255,"%s",array[0]);
	g_strfreev(array);
	create_cfile();
	g_snprintf(cfile->file_name,256,"Capture %d",mainw->cap_number);
	g_snprintf(cfile->name,256,"Capture %d",mainw->cap_number++);
	g_snprintf(cfile->type,40,"Frames");
	cfile->progress_start=cfile->start=1;
	cfile->progress_end=cfile->frames=cfile->end=new_frames;
	cfile->pb_fps=cfile->fps=mainw->rec_fps;

	if (mainw->rec_achans>0) {
	  cfile->arate=cfile->arps=mainw->rec_arate;
	  cfile->achans=mainw->rec_achans;
	  cfile->asampsize=mainw->rec_asamps;
	  cfile->signed_endian=mainw->rec_signed_endian;
	}

	g_snprintf(file_name,256,"%s/%s/",prefs->tmpdir,cfile->handle);
	
	com=g_strdup_printf("smogrify fill_and_redo_frames %s %d %d %d %d %.4f %d %d %d",cfile->handle,1,cfile->frames,mainw->foreign_width,mainw->foreign_height,cfile->fps,cfile->arate,cfile->achans,cfile->asampsize);
	unlink(cfile->info_file);
	dummyvar=system(com);
	cfile->nopreview=TRUE;
	if (do_progress_dialog(TRUE,TRUE,_ ("Cleaning up clip"))) {
	  array=g_strsplit(mainw->msg,"|",5);
	  new_frames=atoi(array[1]);
	  cfile->hsize=atoi(array[2]);
	  cfile->vsize=atoi(array[3]);
	  cfile->bpp=24;
	  g_strfreev(array);
	  
	  if (new_frames>0) {
	    cfile->start=1;
	    cfile->end=cfile->frames=new_frames;
	    get_next_free_file();
	  }
	  else {
	    // failed cleanup
	    cfile->frames=cfile->start=cfile->end=new_frames=0;
	  }
	}
	else {
	  // cancelled cleanup
	  cfile->frames=cfile->start=cfile->end=new_frames=0;
	}
	g_free(com);
      }
    }
    close(capture_fd);
    unlink(capfile);
  }
  else {
    // nothing captured
    g_free(capfile);
    if (mainw->current_file>0) {
      switch_to_file (old_file,old_file);
      cfile->nopreview=FALSE;
    }
    else {
      close_current_file(old_file);
    }
    return FALSE;
  }
  cfile->nopreview=FALSE;
  g_free(capfile);

  if (new_frames==0) {
    // failed to get any frames from this clip
    close_current_file(old_file);
    return FALSE;
  }

  add_to_winmenu();
  switch_to_file(old_file,mainw->current_file);
  cfile->is_loaded=TRUE;
  cfile->changed=TRUE;
  save_clip_values(mainw->current_file);
  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
#ifdef ENABLE_OSC
  lives_osc_notify(LIVES_OSC_NOTIFY_CLIP_OPENED,"");
#endif
  return TRUE;
}


void
set_menu_text(GtkWidget *menuitem, const gchar *text, gboolean use_mnemonic) {
  GtkWidget *label;
  if (GTK_IS_MENU_ITEM (menuitem)) {
    label=gtk_bin_get_child(GTK_BIN(menuitem));
    if (use_mnemonic) {
      gtk_label_set_text_with_mnemonic(GTK_LABEL(label),text);
    }
    else {
      gtk_label_set_text(GTK_LABEL(label),text);
    }
  }
}


void
get_menu_text(GtkWidget *menuitem, gchar *text) {
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(menuitem));
  g_snprintf(text,255,"%s",gtk_label_get_text(GTK_LABEL(label)));
}

void
get_menu_text_long(GtkWidget *menuitem, gchar *text) {
  GtkWidget *label=gtk_bin_get_child(GTK_BIN(menuitem));
  g_snprintf(text,32768,"%s",gtk_label_get_text(GTK_LABEL(label)));
}


inline gboolean int_array_contains_value(int *array, int num_elems, int value) {
  int i;
  for (i=0;i<num_elems;i++) {
    if (array[i]==value) return TRUE;
  }
  return FALSE;
}


void 
reset_clip_menu (void) {
  // sometimes the clip menu gets messed up, e.g. after reloading a set.
  // This function will clean up the 'x's and so on.

  int i;
  GtkWidget *active_image=NULL;

  for (i=1;i<=MAX_FILES;i++) {
    if (!(mainw->files[i]==NULL)) {
      if (!(active_image==NULL)) {
	active_image=NULL;
      }
      if (mainw->files[i]->opening) {
	active_image = gtk_image_new_from_stock ("gtk-no", GTK_ICON_SIZE_MENU);
      }
      else {
	if (i==mainw->current_file) {
	  active_image = gtk_image_new_from_stock ("gtk-close", GTK_ICON_SIZE_MENU);
	}
      }
      if (!(active_image==NULL)) {
	gtk_widget_show (active_image);
      }
      if (mainw->files[i]->menuentry!=NULL) {
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (mainw->files[i]->menuentry), active_image);
      }
    }
  }
}



gboolean
check_file(const gchar *file_name, gboolean check_existing) {
  int check;
  gboolean exists=FALSE;
  gchar *msg;

  // check if file exists
  if (g_file_test (file_name, G_FILE_TEST_EXISTS)) {
    if (check_existing) {
      msg=g_strdup_printf (_ ("\n%s\nalready exists.\n\nOverwrite ?\n"),file_name);
      if (!do_warning_dialog(msg)) {
	g_free (msg);
	return FALSE;
      }
      g_free (msg);
    }
    check=open(file_name,O_RDWR);
    exists=TRUE;
  }
  // if not, check if we can write to it
  else {
    check=open(file_name,O_CREAT|O_EXCL|O_RDWR,S_IRUSR|S_IWUSR);
  }

  if (check<0) {
    if (errno==EACCES&&mainw!=NULL&&mainw->is_ready) {
      g_snprintf(mainw->msg,512,_ ("\nLiVES was unable to write to the file:\n%s\nPlease check the file permissions and try again."),file_name);
      do_error_dialog(mainw->msg);
    }
    return FALSE;
  }
  if (check) {
    close(check);
    if (!exists) {
      unlink (file_name);
    }
    return TRUE;
  }
  return FALSE;
}


gboolean 
check_dir_access (gchar *dir) {
  // if a directory exists, make sure it is readable and writable
  // otherwise create it and then check
  gboolean exists=g_file_test (dir, G_FILE_TEST_EXISTS);
  gchar *com;
  gchar *testfile;
  gboolean is_OK=FALSE;

  if (!exists) {
    com=g_strdup_printf ("/bin/mkdir -p %s",dir);
    dummyvar=system (com);
    g_free (com);
  }
  if (!g_file_test(dir, G_FILE_TEST_IS_DIR)) return FALSE;

  testfile=g_strdup_printf ("%s/livestst.txt",dir);
  com=g_strdup_printf ("touch %s",testfile);
  dummyvar=system (com);
  g_free (com);
  if ((is_OK=g_file_test(testfile, G_FILE_TEST_EXISTS))) {
    unlink (testfile);
  }
  g_free (testfile);
  if (!exists) {
    com=g_strdup_printf ("rm -r %s",dir);
    dummyvar=system (com);
    g_free (com);
  }
  return is_OK;
}



void activate_url_inner(const gchar *link) {
#ifdef HAVE_GTK_NICE_VERSION
  GError *err=NULL;
  gtk_show_uri(NULL,link,GDK_CURRENT_TIME,&err);
#else
  gchar *com = getenv("BROWSER");
  com = g_strdup_printf("%s '%s' &", com ? com : "gnome-open", link);
  dummyvar=system(com);
  g_free(com);
#endif
}


void activate_url (GtkAboutDialog *about, const gchar *link, gpointer data) {
  activate_url_inner(link);
}


void show_manual_section (const gchar *lang, const gchar *section) {
  gchar *tmp=NULL,*tmp2=NULL;
  const gchar *link;

  link=g_strdup_printf("%s%s%s%s",LIVES_MANUAL_URL,(lang==NULL?"":(tmp2=g_strdup_printf("//%s//",lang))),LIVES_MANUAL_FILENAME,(section==NULL?"":(tmp=g_strdup_printf("#%s",section))));

  activate_url_inner(link);

  if (tmp!=NULL) g_free(tmp);
  if (tmp2!=NULL) g_free(tmp2);

}


glong
get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  fstat(fd,&filestat);
  return (gulong)(filestat.st_size);
}

glong
sget_file_size(gchar *name) {
  // get the size of file fd
  struct stat filestat;
  int fd;

  if ((fd=open(name,O_RDONLY))>0) {
    close (fd);
  }
  else {
    return (guint)0;
  }

  stat(name,&filestat);
  return (gulong)(filestat.st_size);
}


void reget_afilesize (int fileno) {
  // re-get the audio file size
  gchar *afile;
  file *sfile=mainw->files[fileno];

  if (mainw->multitrack!=NULL) return; // otherwise achans gets set to 0...

  if (!sfile->opening) afile=g_strdup_printf ("%s/%s/audio",prefs->tmpdir,sfile->handle);
  else afile=g_strdup_printf ("%s/%s/audiodump.pcm",prefs->tmpdir,sfile->handle);
  if ((mainw->multitrack==NULL||fileno!=mainw->multitrack->render_file)&&(sfile->afilesize=sget_file_size (afile))==0l) {
    if (!sfile->opening) {
      if (sfile->arate!=0||sfile->achans!=0||sfile->asampsize!=0||sfile->arps!=0) {
	sfile->arate=sfile->achans=sfile->asampsize=sfile->arps=0;
	save_clip_value(fileno,CLIP_DETAILS_ACHANS,&sfile->achans);
	save_clip_value(fileno,CLIP_DETAILS_ARATE,&sfile->arps);
	save_clip_value(fileno,CLIP_DETAILS_PB_ARATE,&sfile->arate);
	save_clip_value(fileno,CLIP_DETAILS_ASAMPS,&sfile->asampsize);
      }
    }
  }
  g_free (afile);
}





void
colour_equal(GdkColor *c1, const GdkColor *c2) {
  c1->pixel=c2->pixel;
  c1->red=c2->red;
  c1->green=c2->green;
  c1->blue=c2->blue;
}


gboolean
create_event_space(gint length) {
  // try to create desired events
  // if we run out of memory, all events requested are freed, and we return FALSE
  // otherwise we return TRUE

  // NOTE: this is the OLD event system, it's only used for reordering in the clip editor

  if (cfile->events[0]!=NULL) {
    g_free(cfile->events[0]);
  }
  if ((cfile->events[0]=(event *)(g_try_malloc(sizeof(event)*length)))==NULL) {
    // memory overflow
    return FALSE;
  }
  memset(cfile->events[0],0,length*sizeof(event));
  return TRUE;
}



gint lives_list_index (GList *list, const gchar *data) {
  // find data in list, GTK's version is broken

  int i;
  gint len;
  if (list==NULL) return -1;

  len=g_list_length (list);

  for (i=0;i<len;i++) {
    if (!strcmp (g_list_nth_data (list,i),data)) return i;
  }
  return -1;
}






void 
add_to_recent(const gchar *filename, gdouble start, gint frames, const gchar *extra_params) {
  gchar buff[256];
  gchar *file;

  if (frames>0) {
    if (extra_params==NULL||(strlen(extra_params)==0)) file=g_strdup_printf ("%s|%.2f|%d",filename,start,frames);
    else file=g_strdup_printf ("%s|%.2f|%d\n%s",filename,start,frames,extra_params);
  }
  else {
    if (extra_params==NULL||(strlen(extra_params)==0)) file=g_strdup (filename);
    else file=g_strdup_printf ("%s\n%s",filename,extra_params);
  }


  get_menu_text(mainw->recent1,buff);
  if (strcmp(file,buff)) {
    get_menu_text(mainw->recent2,buff);
    if (strcmp(file,buff)) {
      get_menu_text(mainw->recent3,buff);
      if (strcmp(file,buff)) {
	// not in list, or at pos 4
	get_menu_text(mainw->recent3,buff);
	set_menu_text(mainw->recent4,buff,FALSE);
	set_pref("recent4",buff);
	
	get_menu_text(mainw->recent2,buff);
	set_menu_text(mainw->recent3,buff,FALSE);
	set_pref("recent3",buff);
	
	get_menu_text(mainw->recent1,buff);
	set_menu_text(mainw->recent2,buff,FALSE);
	set_pref("recent2",buff);
	
	set_menu_text(mainw->recent1,file,FALSE);
	set_pref("recent1",file);
      }
      else {
	// #3 in list
	get_menu_text(mainw->recent2,buff);
	set_menu_text(mainw->recent3,buff,FALSE);
	set_pref("recent3",buff);
	
	get_menu_text(mainw->recent1,buff);
	set_menu_text(mainw->recent2,buff,FALSE);
	set_pref("recent2",buff);
	
	set_menu_text(mainw->recent1,file,FALSE);
	set_pref("recent1",filename);
      }
    }
    else {
      // #2 in list
      get_menu_text(mainw->recent1,buff);
      set_menu_text(mainw->recent2,buff,FALSE);
      set_pref("recent2",buff);
	
      set_menu_text(mainw->recent1,file,FALSE);
      set_pref("recent1",filename);
    }
  }
  else {
    // I'm number 1, so why change ;-)
  }

  get_menu_text(mainw->recent1,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent1);
  }
  get_menu_text(mainw->recent2,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent2);
  }
  get_menu_text(mainw->recent3,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent3);
  }
  get_menu_text(mainw->recent4,buff);
  if (strlen(buff)) {
    gtk_widget_show (mainw->recent4);
  }

  g_free(file);
}


void lives_set_cursor_style(gint cstyle, GdkWindow *window) {
  if (mainw->cursor!=NULL) gdk_cursor_unref(mainw->cursor);
  mainw->cursor=NULL;

  if (window==NULL) window=mainw->LiVES->window;

  switch(cstyle) {
  case LIVES_CURSOR_NORMAL:
    gdk_window_set_cursor (window, NULL);
    return;
  case LIVES_CURSOR_BUSY:
    mainw->cursor=gdk_cursor_new(GDK_WATCH);
    gdk_window_set_cursor (window, mainw->cursor);
    return;
  }
}




gint 
verhash (gchar *version) {
  gchar *s;
  gint major=0;
  gint minor=0;
  gint micro=0;

  if (!(strlen(version))) return 0;

  s=strtok (version,".");
  if (!(s==NULL)) {
    major=atoi (s);
    s=strtok (NULL,".");
    if (!(s==NULL)) {
      minor=atoi (s);
      s=strtok (NULL,".");
      if (!(s==NULL)) {
	micro=atoi (s);
      }
    }
  }
  return major*1000000+minor*1000+micro;
}



// TODO - move into undo.c
void 
set_undoable (const gchar *what, gboolean sensitive) {
  if (mainw->current_file>-1) {
    cfile->redoable=FALSE;
    cfile->undoable=sensitive;
    if (!(what==NULL)) {
      gchar *what_safe=g_strdelimit (g_strdup (what),"_",' ');
      g_snprintf(cfile->undo_text,32,_ ("_Undo %s"),what_safe);
      g_snprintf(cfile->redo_text,32,_ ("_Redo %s"),what_safe);
      g_free (what_safe);
    }
    else {
      cfile->undoable=FALSE;
      cfile->undo_action=UNDO_NONE;
      g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
      g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));
    }
    set_menu_text(mainw->undo,cfile->undo_text,TRUE);
    set_menu_text(mainw->redo,cfile->redo_text,TRUE);
  }

  gtk_widget_hide(mainw->redo);
  gtk_widget_show(mainw->undo);
  gtk_widget_set_sensitive (mainw->undo,sensitive);
}

void 
set_redoable (const gchar *what, gboolean sensitive) {
  if (mainw->current_file>-1) {
    cfile->undoable=FALSE;
    cfile->redoable=sensitive;
    if (!(what==NULL)) {
      gchar *what_safe=g_strdelimit (g_strdup (what),"_",' ');
      g_snprintf(cfile->undo_text,32,_ ("_Undo %s"),what_safe);
      g_snprintf(cfile->redo_text,32,_ ("_Redo %s"),what_safe);
      g_free (what_safe);
    }
    else {
      cfile->redoable=FALSE;
      cfile->undo_action=UNDO_NONE;
      g_snprintf(cfile->undo_text,32,"%s",_ ("_Undo"));
      g_snprintf(cfile->redo_text,32,"%s",_ ("_Redo"));
    }
    set_menu_text(mainw->undo,cfile->undo_text,TRUE);
    set_menu_text(mainw->redo,cfile->redo_text,TRUE);
  }

  gtk_widget_hide(mainw->undo);
  gtk_widget_show(mainw->redo);
  gtk_widget_set_sensitive (mainw->redo,sensitive);
}


void 
set_sel_label (GtkWidget *sel_label) {
  gchar *tstr,*frstr,*tmp;
  gchar *sy,*sz;

  if (mainw->current_file==-1||!cfile->frames||mainw->multitrack!=NULL) {
    gtk_label_set_text(GTK_LABEL(sel_label),_ ("-------------Selection------------"));
  }
  else {
    tstr=g_strdup_printf ("%.2f",calc_time_from_frame (mainw->current_file,cfile->end+1)-calc_time_from_frame (mainw->current_file,cfile->start));
    frstr=g_strdup_printf ("%d",cfile->end-cfile->start+1);
    gtk_label_set_text(GTK_LABEL(sel_label),(tmp=g_strconcat ("---------- [ ",tstr,(sy=(g_strdup(_(" sec ] ----------Selection---------- [ ")))),frstr,(sz=g_strdup(_(" frames ] ----------"))),NULL)));
    g_free(sy);
    g_free(sz);

    g_free (tmp);
    g_free (frstr);
    g_free (tstr);
  }
  gtk_widget_queue_draw (sel_label);
}




inline void g_list_free_strings(GList *slist) {
  GList *list=slist;
  if (list==NULL) return;
  while (list!=NULL) {
    if (list->data!=NULL) {
      //g_printerr("free %s\n",list->data);
      g_free(list->data);
    }
    list=list->next;
  }
}


void cache_file_contents(gchar *filename) {
  FILE *hfile;
  gchar buff[65536];

  if (mainw->cached_list!=NULL) {
    g_list_free_strings(mainw->cached_list);
    g_list_free(mainw->cached_list);
    mainw->cached_list=NULL;
  }

  if (!(hfile=fopen(filename,"r"))) return;
  while (fgets(buff,65536,hfile)!=NULL) {
    pthread_mutex_lock(&mainw->gtk_mutex);
    mainw->cached_list=g_list_append(mainw->cached_list,g_strdup(buff));
    pthread_mutex_unlock(&mainw->gtk_mutex);
  }
  fclose(hfile);
}


gchar *get_val_from_cached_list(const gchar *key, size_t maxlen) {
  GList *clist=mainw->cached_list;
  gchar *keystr_start=g_strdup_printf("<%s>",key);
  gchar *keystr_end=g_strdup_printf("</%s>",key);
  size_t kslen=strlen(keystr_start);
  size_t kelen=strlen(keystr_end);

  gboolean gotit=FALSE;
  gchar buff[maxlen];

  memset(buff,0,1);

  while (clist!=NULL) {
    if (gotit) {
      if (!strncmp(keystr_end,clist->data,kelen)) {
	if (clist->prev!=NULL) clist->prev->next=clist->next;
	break;
      }
      if (strncmp(clist->data,"#",1)) g_strappend(buff,maxlen,clist->data);
      else {
	if (clist->prev!=NULL) clist->prev->next=clist->next;
      }
    }
    else if (!strncmp(keystr_start,clist->data,kslen)) {
      gotit=TRUE;
      if (clist->prev!=NULL) clist->prev->next=clist->next;
    }
    clist=clist->next;
  }
  g_free(keystr_start);
  g_free(keystr_end);

  if (strlen(buff)>0) memset(buff+strlen(buff)-1,0,1); // remove trailing newline

  return g_strdup(buff);
}



gboolean get_clip_value(int which, int what, void *retval, size_t maxlen) {
  FILE *valfile;
  gchar *vfile;
  gchar *lives_header=NULL;
  gchar *old_header;
  gchar *com;
  gint timeout=PREFS_TIMEOUT/prefs->sleep_time;
  gchar *val;
  gchar *key;
  time_t old_time=0,new_time=0;
  struct stat mystat;

  if (mainw->cached_list==NULL) {
    
    lives_header=g_strdup_printf("%s/%s/header.lives",prefs->tmpdir,mainw->files[which]->handle);
    old_header=g_strdup_printf("%s/%s/header",prefs->tmpdir,mainw->files[which]->handle);
    
    // TODO - remove this some time before 2038
    if (!stat(old_header,&mystat)) old_time=mystat.st_mtime;
    if (!stat(lives_header,&mystat)) new_time=mystat.st_mtime;
    g_free(old_header);
    
    if (old_time>new_time) {
      g_free(lives_header);
      return FALSE; // clip has been edited by an older version of LiVES
    }
  }
  //////////////////////////////////////////////////

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key=g_strdup("header_version");
    maxlen=256;
    break;
  case CLIP_DETAILS_BPP:
    key=g_strdup("bpp");
    maxlen=256;
    break;
  case CLIP_DETAILS_FPS:
    key=g_strdup("fps");
    maxlen=256;
    break;
  case CLIP_DETAILS_PB_FPS:
    key=g_strdup("pb_fps");
    maxlen=256;
    break;
  case CLIP_DETAILS_WIDTH:
    key=g_strdup("width");
    maxlen=256;
    break;
  case CLIP_DETAILS_HEIGHT:
    key=g_strdup("height");
    maxlen=256;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    key=g_strdup("unique_id");
    maxlen=256;
    break;
  case CLIP_DETAILS_ARATE:
    key=g_strdup("audio_rate");
    maxlen=256;
    break;
  case CLIP_DETAILS_PB_ARATE:
    key=g_strdup("pb_audio_rate");
    maxlen=256;
    break;
  case CLIP_DETAILS_ACHANS:
    key=g_strdup("audio_channels");
    maxlen=256;
    break;
  case CLIP_DETAILS_ASIGNED:
    key=g_strdup("audio_signed");
    maxlen=256;
    break;
  case CLIP_DETAILS_AENDIAN:
    key=g_strdup("audio_endian");
    maxlen=256;
    break;
  case CLIP_DETAILS_ASAMPS:
    key=g_strdup("audio_sample_size");
    maxlen=256;
    break;
  case CLIP_DETAILS_FRAMES:
    key=g_strdup("frames");
    maxlen=256;
    break;
  case CLIP_DETAILS_TITLE:
    key=g_strdup("title");
    break;
  case CLIP_DETAILS_AUTHOR:
    key=g_strdup("author");
    break;
  case CLIP_DETAILS_COMMENT:
    key=g_strdup("comment");
    break;
  case CLIP_DETAILS_KEYWORDS:
    key=g_strdup("keywords");
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    key=g_strdup("pb_frameno");
    maxlen=256;
    break;
  case CLIP_DETAILS_CLIPNAME:
    key=g_strdup("clipname");
    break;
  case CLIP_DETAILS_FILENAME:
    key=g_strdup("filename");
    break;
  default:
    g_printerr("invalid detail %d requested from file %s",which,lives_header);
    if (lives_header!=NULL) g_free(lives_header);
    return FALSE;
  }

  if (mainw->cached_list!=NULL) {
    val=get_val_from_cached_list(key,maxlen);
    g_free(key);
  }
  else {
    com=g_strdup_printf("smogrify get_clip_value %s %d %d %s",key,getuid(),getpid(),lives_header);
    g_free(lives_header);
    g_free(key);
    
    val=g_malloc(maxlen);
    memset(val,0,maxlen);
    
    pthread_mutex_lock(&mainw->gtk_mutex);
    if (system(com)) {
      tempdir_warning();
      pthread_mutex_unlock(&mainw->gtk_mutex);
      g_free(com);
      return FALSE;
    }
    
    vfile=g_strdup_printf("%s/.smogval.%d.%d",prefs->tmpdir,getuid(),getpid());
    pthread_mutex_unlock(&mainw->gtk_mutex);
    do {
      if (!(valfile=fopen(vfile,"r"))) {
	if (!(mainw==NULL)) {
	  pthread_mutex_lock(&mainw->gtk_mutex);
	  while (g_main_context_iteration(NULL,FALSE));
	  pthread_mutex_unlock(&mainw->gtk_mutex);
	}
	g_usleep(prefs->sleep_time);
	timeout--;
      }
    } while (!valfile&&timeout>0);
    
    if (timeout<=0) {
      tempdir_warning();
      g_free(val);
      g_free(vfile);
      g_free(com);
      return FALSE;
    }
    else {
      dummychar=fgets(val,maxlen-1,valfile);
      fclose(valfile);
      unlink(vfile);
    }
    g_free(vfile);
    g_free(com);
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_WIDTH:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES:
  case CLIP_DETAILS_HEADER_VERSION:
    *(gint *)retval=atoi(val);
    break;
  case CLIP_DETAILS_ASIGNED:
    *(gint *)retval=0;
    if (mainw->files[which]->header_version==0) *(gint *)retval=atoi(val);
    if (*(gint *)retval==0&&(!strcasecmp(val,"false"))) *(gint *)retval=1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(gint *)retval=atoi(val);
    if (retval==0) *(gint *)retval=1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(gint *)retval=atoi(val);
    if (retval==0) *(gint *)retval=mainw->files[which]->arate;
    break;
  case CLIP_DETAILS_FPS:
    *(gdouble *)retval=strtod(val,NULL);
    if (*(gdouble *)retval==0.) *(gdouble *)retval=prefs->default_fps;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(gdouble *)retval=strtod(val,NULL);
    if (*(gdouble *)retval==0.) *(gdouble *)retval=mainw->files[which]->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    if (capable->cpu_bits==32) {
      *(gint64 *)retval=strtoll(val,NULL,10);
    }
    else {
      *(gint64 *)retval=strtol(val,NULL,10);
    }
    break;
  case CLIP_DETAILS_AENDIAN:
    *(gint *)retval=atoi(val)*2;
    break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_FILENAME:
  case CLIP_DETAILS_KEYWORDS:
    g_snprintf(retval,maxlen,"%s",val);
    break;
  }
  g_free(val);
  return TRUE;
}



void save_clip_value(int which, int what, void *val) {
  gchar *lives_header;
  gchar *com;
  gchar *myval;
  gchar *key;

  if (which==0||which==mainw->scrap_file) return;

  lives_header=g_strdup_printf("%s/%s/header.lives",prefs->tmpdir,mainw->files[which]->handle);
    
  switch (what) {
  case CLIP_DETAILS_BPP:
    key=g_strdup("bpp");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_FPS:
    key=g_strdup("fps");
    if (!mainw->files[which]->ratio_fps) myval=g_strdup_printf("%.3f",*(gdouble *)val);
    else myval=g_strdup_printf("%.8f",*(gdouble *)val);
    break;
  case CLIP_DETAILS_PB_FPS:
    key=g_strdup("pb_fps");
    if (mainw->files[which]->ratio_fps&&(mainw->files[which]->pb_fps==mainw->files[which]->fps)) myval=g_strdup_printf("%.8f",*(gdouble *)val);
    else myval=g_strdup_printf("%.3f",*(gdouble *)val);
    break;
  case CLIP_DETAILS_WIDTH:
    key=g_strdup("width");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_HEIGHT:
    key=g_strdup("height");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    key=g_strdup("unique_id");
    if (capable->cpu_bits==32) {
      myval=g_strdup_printf("%lld",*(gint64 *)val);
    }
    else {
      myval=g_strdup_printf("%ld",*(gint64 *)val);
    }
    break;
  case CLIP_DETAILS_ARATE:
    key=g_strdup("audio_rate");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_PB_ARATE:
    key=g_strdup("pb_audio_rate");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_ACHANS:
    key=g_strdup("audio_channels");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_ASIGNED:
    key=g_strdup("audio_signed");
    if (*(gint *)val==1) myval=g_strdup("true");
    else myval=g_strdup("false");
    break;
  case CLIP_DETAILS_AENDIAN:
    key=g_strdup("audio_endian");
    myval=g_strdup_printf("%d",*(gint *)val/2);
    break;
  case CLIP_DETAILS_ASAMPS:
    key=g_strdup("audio_sample_size");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_FRAMES:
    key=g_strdup("frames");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_TITLE:
    key=g_strdup("title");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_AUTHOR:
    key=g_strdup("author");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_COMMENT:
    key=g_strdup("comment");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_KEYWORDS:
    key=g_strdup("keywords");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    key=g_strdup("pb_frameno");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  case CLIP_DETAILS_CLIPNAME:
    key=g_strdup("clipname");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_FILENAME:
    key=g_strdup("filename");
    myval=g_strdup(val);
    break;
  case CLIP_DETAILS_HEADER_VERSION:
    key=g_strdup("header_version");
    myval=g_strdup_printf("%d",*(gint *)val);
    break;
  default:
    g_printerr("invalid detail %d set in file %s",which,lives_header);
    g_free(lives_header);
    return;
  }

  if (mainw->clip_header!=NULL) {
    gchar *keystr_start=g_strdup_printf("<%s>\n",key);
    gchar *keystr_end=g_strdup_printf("\n</%s>\n",key);
    fputs(keystr_start,mainw->clip_header);
    fputs(myval,mainw->clip_header);
    fputs(keystr_end,mainw->clip_header);
    g_free(keystr_start);
    g_free(keystr_end);
  }
  else {
    com=g_strdup_printf("smogrify set_clip_value %s %s \"%s\"",lives_header,key,myval);
    dummyvar=system(com);
    g_free(com);
  }

  g_free(lives_header);
  g_free(myval);
  g_free(key);
  
  return;
}



GList *get_set_list(const gchar *dir) {
  // get list of sets in top level dir
  GList *setlist=NULL;
  DIR *tldir,*subdir;
  struct dirent *tdirent,*subdirent;
  gchar *subdirname;

  if (dir==NULL) return NULL;

  tldir=opendir(dir);

  if (tldir==NULL) return NULL;

  while (1) {
    tdirent=readdir(tldir);

    if (tdirent==NULL) {
      closedir(tldir);
      return setlist;
    }
    
    if (!strncmp(tdirent->d_name,"..",strlen(tdirent->d_name))) continue;

    subdirname=g_strdup_printf("%s/%s",dir,tdirent->d_name);

    subdir=opendir(subdirname);

    if (subdir==NULL) {
      g_free(subdirname);
      continue;
    }

    while (1) {
      subdirent=readdir(subdir);

      if (subdirent==NULL) {
	break;
      }

      if (!strcmp(subdirent->d_name,"order")) {
	setlist=g_list_append(setlist,g_strdup(tdirent->d_name));
	break;
      }
    }
    g_free(subdirname);
    closedir(subdir);
  }
}




gboolean check_for_ratio_fps (gdouble fps) {
  gboolean ratio_fps;
  gchar *test_fps_string1=g_strdup_printf ("%.3f00000",fps);
  gchar *test_fps_string2=g_strdup_printf ("%.8f",fps);
  
  if (strcmp (test_fps_string1,test_fps_string2)) {
    // got a ratio
    ratio_fps=TRUE;
  }
  else {
    ratio_fps=FALSE;
  }
  g_free (test_fps_string1);
  g_free (test_fps_string2);

  return ratio_fps;
}


gdouble get_ratio_fps(gchar *string) {
  // return a ratio (8dp) fps from a string with format num:denom
  gdouble fps;
  gchar *fps_string;
  gchar **array=g_strsplit(string,":",2);
  gint num=atoi (array[0]);
  gint denom=atoi (array[1]);
  g_strfreev (array);
  fps=(gdouble)num/(gdouble)denom;
  fps_string=g_strdup_printf("%.8f",fps);
  fps=g_strtod(fps_string,NULL);
  g_free(fps_string);
  return fps;
}



gchar *remove_trailing_zeroes(gdouble val) {
  int i;
  gdouble xval=val;

  if (val==(int)val) return g_strdup_printf("%d",(int)val);
  for (i=0;i<=16;i++) {
    xval*=10.;
    if (xval==(int)xval) return g_strdup_printf("%.*f",i,val);
  }
  return g_strdup_printf("%.*f",i,val);
}


gint real_clips_available(void) {
  int i,count=0;
  for (i=1;i<MAX_FILES;i++) {
    if (mainw->files[i]!=NULL&&(mainw->files[i]->clip_type==CLIP_TYPE_DISK||mainw->files[i]->clip_type==CLIP_TYPE_FILE)) count++;
  }
  return count;
}




guint 
get_signed_endian (int asigned, int endian) {
  if (asigned==1) {
    if (endian==1) {
      return 0;
    }
    if (endian==0) {
      return AFORM_BIG_ENDIAN;
    }
  }
  else {
    if (asigned==0) { 
      if (endian==1) {
	return AFORM_UNSIGNED;
      }
      if (endian==0) {
	return AFORM_UNSIGNED|AFORM_BIG_ENDIAN;
      }
    }
  }
  return AFORM_UNKNOWN;
}




gint 
get_token_count (gchar *string, int delim) {
  gint pieces=1;
  if (string==NULL) return 0;
  if (delim<=0||delim>255) return 1;

  while ((string=strchr(string,delim))!=NULL) {
    pieces++;
    string++;
  }
  return pieces;
}



gchar *subst (gchar *string, gchar *from, gchar *to) {
  // return a string with all occurrences of from replaced with to
  gchar *search,*lastsearch;
  gchar *ret=g_strdup(""),*ret2;

  search=lastsearch=string;

  while (search!=NULL) {
    if ((search=strstr (lastsearch,from))==NULL) {
      ret2=g_strconcat (ret,lastsearch,NULL);
      g_free(ret);
      ret=ret2;
    }
    else {
      memset (search,0,1);
      ret2=g_strconcat (ret,lastsearch,to,NULL);
      g_free(ret);
      ret=ret2;
      lastsearch=search+strlen (from);
    }
  }
  return ret;
}




void combo_set_popdown_strings (GtkCombo *combo, GList *list) {
  // this avoids an assert in some versions of GTK+ when list==NULL
  GList *empty_list=NULL;
  empty_list=g_list_append (empty_list,"");
  if (list==NULL) gtk_combo_set_popdown_strings (combo,empty_list);
  else {
    gtk_combo_set_popdown_strings (combo,g_list_copy(list));
    g_list_free (empty_list);
  }
}


void 
get_border_size (GtkWidget *win, gint *bx, gint *by) {
  GdkRectangle rect;
  gint wx,wy;
  pthread_mutex_lock(&mainw->gtk_mutex);
  gdk_window_get_frame_extents (GDK_WINDOW (win->window),&rect);
  gdk_window_get_origin (GDK_WINDOW (win->window), &wx, &wy);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  bx[0]=wx-rect.x;
  by[0]=wy-rect.y;
}



gint hextodec (const gchar *string) {
  int i;
  gint tot=0;
  gchar test[2];

  memset (test+1,0,1);

  for (i=0;i<strlen (string);i++) {
    tot*=16;
    w_memcpy (test,&string[i],1);
    tot+=get_hex_digit (test);
  }
  return tot;
}

gint get_hex_digit (gchar *c) {
  if (!strcmp (c,"a")||!strcmp (c,"A")) return 10;
  if (!strcmp (c,"b")||!strcmp (c,"B")) return 11;
  if (!strcmp (c,"c")||!strcmp (c,"C")) return 12;
  if (!strcmp (c,"d")||!strcmp (c,"D")) return 13;
  if (!strcmp (c,"e")||!strcmp (c,"E")) return 14;
  if (!strcmp (c,"f")||!strcmp (c,"F")) return 15;
  return(atoi (c));
}



static guint32 fastrand_val;

inline guint32 fastrand(void)
{
#define rand_a 1073741789L
#define rand_c 32749L

  return (fastrand_val= rand_a * fastrand_val + rand_c);
}

void fastsrand(guint32 seed)
{
  fastrand_val = seed;
}




GdkPixmap*
gdk_pixmap_copy (GdkPixmap *pixmap)
{
  GdkPixmap *pixmap_out;
  GdkGC *gc;
  gint width, height, depth;
  
  if (pixmap==NULL) return NULL;
  
  gdk_drawable_get_size (pixmap, &width, &height);
  depth = gdk_drawable_get_depth (pixmap);
  pixmap_out = gdk_pixmap_new (NULL, width, height, depth);
  gc = gdk_gc_new (pixmap);
  gdk_draw_drawable (pixmap_out, gc, pixmap, 0, 0, 0, 0, width, height);
  g_object_unref (gc);
  return pixmap_out;
}


void set_fg_colour(gint red, gint green, gint blue) {
  GdkColor col;
  col.red=red*255;
  col.green=green*255;
  col.blue=blue*255;
  if (mainw->general_gc==NULL) mainw->general_gc=gdk_gc_new(GDK_DRAWABLE(mainw->LiVES->window));
  gdk_gc_set_rgb_fg_color(mainw->general_gc,&col);
}


gboolean
label_act_toggle (GtkWidget *widget, GdkEventButton *event, GtkToggleButton *togglebutton) {
  if (!GTK_WIDGET_IS_SENSITIVE(togglebutton)) return FALSE;
  gtk_toggle_button_set_active (togglebutton, !gtk_toggle_button_get_active(togglebutton));
  return FALSE;
}

gboolean
widget_act_toggle (GtkWidget *widget, GtkToggleButton *togglebutton) {
  gtk_toggle_button_set_active (togglebutton, TRUE);
  return FALSE;
}


void gtk_tooltips_copy(GtkWidget *dest, GtkWidget *source) {
  GtkTooltipsData *td=gtk_tooltips_data_get(source);
  if (td==NULL) return;
  gtk_tooltips_set_tip (td->tooltips, dest, td->tip_text, td->tip_private);
}

