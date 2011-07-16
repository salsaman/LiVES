// plugins.c
// LiVES
// (c) G. Finch 2003 - 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <dlfcn.h>
#include <errno.h>

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-palettes.h"
#include "weed/weed-effects.h"
#include "weed/weed-utils.h"
#include "weed/weed-host.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-utils.h"
#include "../libweed/weed-host.h"
#endif

#include "main.h"
#include "resample.h"
#include "support.h"
#include "effects-weed.h"

#include "rfx-builder.h"
#include "paramwindow.h"

const char *anames[AUDIO_CODEC_MAX]={"mp3","pcm","mp2","vorbis","AC3","AAC","AMR_NB","raw",""};

static gboolean list_plugins;


///////////////////////
// command-line plugins


static GList *get_plugin_result (const gchar *command, const gchar *delim, gboolean allow_blanks) {
  gchar **array;
  gint bytes=0,pieces;
  int outfile_fd,i;
  gchar *msg,*buf;
  gint error;
  GList *list=NULL;
  gint count=30000000/prefs->sleep_time;  // timeout of 30 seconds

  gchar *outfile;
  gchar *com;
  gchar buffer[65536];

  pthread_mutex_lock(&mainw->gtk_mutex);

  outfile=g_strdup_printf ("%s/.smogplugin.%d",prefs->tmpdir,getpid());
  com=g_strconcat (command," >",outfile,NULL);

  mainw->error=FALSE;

  if ((error=system (com))!=0&&error!=126*256&&error!=256) {
    if (!list_plugins) {
      gchar *msg2;
      g_free (com);
      if (mainw->is_ready) {
	if ((outfile_fd=open(outfile,O_RDONLY))) {
	  bytes=read (outfile_fd,&buffer,65535);
	  close (outfile_fd);
	  unlink (outfile);
	  memset (buffer+bytes,0,1);
	}
	msg=g_strdup_printf (_("\nPlugin error: %s failed with code %d"),command,error/256);
	if (bytes) {
	  msg2=g_strconcat (msg,g_strdup_printf (_ (" : message was %s\n"),buffer),NULL);
	}
	else {
	  msg2=g_strconcat (msg,"\n",NULL);
	}
	d_print (msg2);
	g_free (msg2);
	g_free (msg);
      }
    }
    g_free (outfile);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    return list;
  }
  g_free (com);
  if (!g_file_test (outfile, G_FILE_TEST_EXISTS)) {
    g_free (outfile);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    return NULL;
  }
  pthread_mutex_unlock(&mainw->gtk_mutex);

  while ((outfile_fd=open(outfile,O_RDONLY))==-1&&(count-->0||list_plugins)) {
    g_usleep (prefs->sleep_time);
  }
  
  if (!count) {
    pthread_mutex_lock(&mainw->gtk_mutex);
    g_printerr (_("Plugin timed out on message %s\n"),command);
    g_free (outfile);
    pthread_mutex_unlock(&mainw->gtk_mutex);
    return list;
  }
  
  bytes=read (outfile_fd,&buffer,65535);
  close (outfile_fd);
  unlink (outfile);
  pthread_mutex_lock(&mainw->gtk_mutex);
  g_free (outfile);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  memset (buffer+bytes,0,1);

#ifdef DEBUG_PLUGINS
  g_print("plugin msg: %s %d\n",buffer,error);
#endif
  
  if (error==256) {
    mainw->error=TRUE;
    g_snprintf (mainw->msg,512,"%s",buffer);
    return list;
  }
  
  pieces=get_token_count (buffer,delim[0]);
  array=g_strsplit(buffer,delim,pieces);
  for (i=0;i<pieces;i++) {
    if (array[i]!=NULL) {
      buf=g_strdup(g_strchomp (g_strchug(array[i])));
      if (strlen (buf)||allow_blanks) {
	list=g_list_append (list, buf);
      }
      else g_free(buf);
    }
  }
  pthread_mutex_lock(&mainw->gtk_mutex);
  g_strfreev (array);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  return list;
}


GList *
plugin_request_with_blanks (const gchar *plugin_type, const gchar *plugin_name, const gchar *request) {
  // allow blanks in a list
  return plugin_request_common(plugin_type, plugin_name, request, "|", TRUE);
}

GList *
plugin_request (const gchar *plugin_type, const gchar *plugin_name, const gchar *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "|", FALSE);
}

GList *
plugin_request_by_line (const gchar *plugin_type, const gchar *plugin_name, const gchar *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "\n", FALSE);
}
GList *
plugin_request_by_space (const gchar *plugin_type, const gchar *plugin_name, const gchar *request) {
  return plugin_request_common(plugin_type, plugin_name, request, " ", FALSE);
}



GList *
plugin_request_common (const gchar *plugin_type, const gchar *plugin_name, const gchar *request, const gchar *delim, gboolean allow_blanks) {
  // returns a GList of responses to -request, or NULL on error
  // by_line says whether we split on '\n' or on '|'
  GList *reslist=NULL;
  gchar *com;

  if (plugin_type!=NULL) {
    // some types live in home directory...
    if (!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM)||!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_TEST)) {
      com=g_strdup_printf ("%s/%s%s/%s %s",capable->home_dir,LIVES_CONFIG_DIR,plugin_type,plugin_name,request);
    }
    else if (!strcmp(plugin_type,PLUGIN_RFX_SCRAP)) {
      // scraps are in the tmpdir
      com=g_strdup_printf("%s/%s %s",prefs->tmpdir,plugin_name,request);
    }
    else {
#ifdef DEBUG_PLUGINS
      com=g_strdup_printf ("%s%s%s/%s %s",prefs->lib_dir,PLUGIN_EXEC_DIR,plugin_type,plugin_name,request);
#else
      com=g_strdup_printf ("%s%s%s/%s %s 2>/dev/null",prefs->lib_dir,PLUGIN_EXEC_DIR,plugin_type,plugin_name,request);
#endif
    }
    if (plugin_name==NULL||!strlen(plugin_name)) {
      return reslist;
    }
  }
  else com=g_strdup (request);
  list_plugins=FALSE;
  reslist=get_plugin_result (com,delim,allow_blanks);
  pthread_mutex_lock(&mainw->gtk_mutex);
  g_free(com);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  return reslist;
}


//////////////////
// get list of plugins of various types

GList *get_plugin_list (const gchar *plugin_type, gboolean allow_nonex, const gchar *plugdir, const gchar *filter_ext) {
  // returns a GList * of plugins of type plugin_type
  // returns empty list if there are no plugins of that type

  // allow_nonex to allow non-executable files (e.g. libs)
  // filter_ext can be non-NULL to filter for files ending .filter_ext

  // TODO - use enum for plugin_type

  gchar *com,*tmp;
  GList *pluglist;

  const gchar *ext=(filter_ext==NULL)?"":filter_ext;

  if (!strcmp(plugin_type,PLUGIN_THEMES)) {
    com=g_strdup_printf ("smogrify list_plugins 0 1 \"%s%s\" \"\"",prefs->prefix_dir,THEME_DIR);
  }
  else if (!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS)||!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS)||!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM)||!strcmp (plugin_type,PLUGIN_RENDERED_EFFECTS_TEST)) {
    // look in home
    com=g_strdup_printf ("smogrify list_plugins %d 0 \"%s/%s%s\" \"%s\"",allow_nonex,capable->home_dir,LIVES_CONFIG_DIR,plugin_type,ext);
  }
  else if (!strcmp(plugin_type,PLUGIN_EFFECTS_WEED)) {
    com=g_strdup_printf ("smogrify list_plugins 1 1 \"%s\" \"%s\"",(tmp=g_filename_from_utf8(plugdir,-1,NULL,NULL,NULL)),ext);
    g_free(tmp);
  }
  else if (!strcmp(plugin_type,PLUGIN_DECODERS)) {
    com=g_strdup_printf ("smogrify list_plugins 1 0 \"%s\" \"%s\"",(tmp=g_filename_from_utf8(plugdir,-1,NULL,NULL,NULL)),ext);
    g_free(tmp);
  }
  else if (!strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS)) {
    com=g_strdup_printf ("smogrify list_plugins %d 0 \"%s%s%s\" \"%s\"",allow_nonex,prefs->prefix_dir,PLUGIN_SCRIPTS_DIR,plugin_type,ext);
  }
  else {
    com=g_strdup_printf ("smogrify list_plugins %d 0 \"%s%s%s\" \"%s\"",allow_nonex,prefs->lib_dir,PLUGIN_EXEC_DIR,plugin_type,ext);
  }
  list_plugins=TRUE;
  pluglist=get_plugin_result (com,"|",FALSE);
  pthread_mutex_lock(&mainw->gtk_mutex);
  g_free(com);
  pthread_mutex_unlock(&mainw->gtk_mutex);
  return pluglist;
}



///////////////////
// video plugins


void save_vpp_defaults(_vid_playback_plugin *vpp, gchar *vpp_file) {
  // format is:
  // nbytes (string) : LiVES vpp defaults file version 2\n
  // for each video playback plugin:
  // 4 bytes (int) name length
  // n bytes name
  // 4 bytes (int) version length
  // n bytes version
  // 4 bytes (int) palette
  // 4 bytes (int) YUV sampling
  // 4 bytes (int) YUV clamping
  // 4 bytes (int) YUV subspace
  // 4 bytes (int) width
  // 4 bytes (int) height
  // 8 bytes (double) fps
  // 4 bytes (int) fps_numerator [0 indicates use fps double, >0 use fps_numer/fps_denom ]
  // 4 bytes (int) fps_denominator
  // 4 bytes argc (count of argv, may be 0)
  //
  // for each argv (extra params):
  // 4 bytes (int) length
  // n bytes string param value

  int fd;
  gint32 len;
  const gchar *version;
  int i;
  gchar *msg;
  int intzero=0;
  gdouble dblzero=0.;

  if (mainw->vpp==NULL) {
    unlink(vpp_file);
    return;
  }

  if ((fd=open(vpp_file,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
    msg=g_strdup_printf (_("\n\nUnable to write video playback plugin defaults file\n%s\nError code %d\n"),vpp_file,errno);
    g_printerr ("%s",msg);
    g_free (msg);
    return;
  }

  g_printerr(_("Updating video playback plugin defaults in %s\n"),vpp_file);
	     
  msg=g_strdup("LiVES vpp defaults file version 2\n");
  dummyvar=write(fd,msg,strlen(msg));
  g_free(msg);

  len=strlen(mainw->vpp->name);
  dummyvar=write(fd,&len,sizint);
  dummyvar=write(fd,mainw->vpp->name,len);

  version=(*mainw->vpp->version)();
  len=strlen(version);
  dummyvar=write(fd,&len,sizint);
  dummyvar=write(fd,version,len);

  dummyvar=write(fd,&(mainw->vpp->palette),sizint);
  dummyvar=write(fd,&(mainw->vpp->YUV_sampling),sizint);
  dummyvar=write(fd,&(mainw->vpp->YUV_clamping),sizint);
  dummyvar=write(fd,&(mainw->vpp->YUV_subspace),sizint);

  dummyvar=write(fd,mainw->vpp->fwidth<=0?&intzero:&(mainw->vpp->fwidth),sizint);
  dummyvar=write(fd,mainw->vpp->fheight<=0?&intzero:&(mainw->vpp->fheight),sizint);

  dummyvar=write(fd,mainw->vpp->fixed_fpsd<=0.?&dblzero:&(mainw->vpp->fixed_fpsd),sizdbl);
  dummyvar=write(fd,mainw->vpp->fixed_fps_numer<=0?&intzero:&(mainw->vpp->fixed_fps_numer),sizint);
  dummyvar=write(fd,mainw->vpp->fixed_fps_denom<=0?&intzero:&(mainw->vpp->fixed_fps_denom),sizint);

  dummyvar=write(fd,&(mainw->vpp->extra_argc),sizint);

  for (i=0;i<mainw->vpp->extra_argc;i++) {
    len=strlen(mainw->vpp->extra_argv[i]);
    dummyvar=write(fd,&len,sizint);
    dummyvar=write(fd,mainw->vpp->extra_argv[i],len);
  }

  close(fd);

}


void load_vpp_defaults(_vid_playback_plugin *vpp, gchar *vpp_file) {
  int fd;
  gint32 len;
  const gchar *version;
  gchar buf[512];
  int i;
  gchar *msg;

  if (!g_file_test(vpp_file,G_FILE_TEST_EXISTS)) {
    return;
  }

  msg=g_strdup_printf(_("Loading video playback plugin defaults from %s..."),vpp_file);
  d_print(msg);
  g_free(msg);

  if ((fd=open(vpp_file,O_RDONLY))==-1) {
    msg=g_strdup_printf (_("unable to read file\n%s\nError code %d\n"),vpp_file,errno);
    d_print (msg);
    g_free (msg);
    return;
  }

  msg=g_strdup("LiVES vpp defaults file version 2\n");
  len=read(fd,buf,strlen(msg));
  memset(buf+len,0,1);
  
  // identifier string
  if (strcmp(msg,buf)) {
    g_free(msg);
    d_print_file_error_failed();
    return;
  }
  g_free(msg);

  // plugin name
  dummyvar=read(fd,&len,sizint);
  dummyvar=read(fd,buf,len);
  memset(buf+len,0,1);

  if (strcmp(buf,mainw->vpp->name)) {
    d_print_failed();
    return;
  }

  // version string
  version=(*mainw->vpp->version)();
  dummyvar=read(fd,&len,sizint);
  dummyvar=read(fd,buf,len);
  memset(buf+len,0,1);

  if (strcmp(buf,version)) {
    msg=g_strdup_printf(_("\nThe %s video playback plugin has been updated.\nPlease check your settings in\n Tools|Preferences|Playback|Playback Plugins Advanced\n\n"),mainw->vpp->name);
    do_error_dialog(msg);
    g_free(msg);
    unlink(vpp_file);
    d_print_failed();
    return;
  }

  dummyvar=read(fd,&(mainw->vpp->palette),sizint);
  dummyvar=read(fd,&(mainw->vpp->YUV_sampling),sizint);
  dummyvar=read(fd,&(mainw->vpp->YUV_clamping),sizint);
  dummyvar=read(fd,&(mainw->vpp->YUV_subspace),sizint);
  dummyvar=read(fd,&(mainw->vpp->fwidth),sizint);
  dummyvar=read(fd,&(mainw->vpp->fheight),sizint);
  dummyvar=read(fd,&(mainw->vpp->fixed_fpsd),sizdbl);
  dummyvar=read(fd,&(mainw->vpp->fixed_fps_numer),sizint);
  dummyvar=read(fd,&(mainw->vpp->fixed_fps_denom),sizint);

  dummyvar=read(fd,&(mainw->vpp->extra_argc),sizint);
  
  if (vpp->extra_argv!=NULL) {
    for (i=0;vpp->extra_argv[i]!=NULL;i++) {
      g_free(vpp->extra_argv[i]);
    }
    g_free(vpp->extra_argv);
  }

  mainw->vpp->extra_argv=g_malloc((mainw->vpp->extra_argc+1)*(sizeof(gchar *)));

  for (i=0;i<mainw->vpp->extra_argc;i++) {
    dummyvar=read(fd,&len,sizint);
    mainw->vpp->extra_argv[i]=g_malloc(len+1);
    dummyvar=read(fd,mainw->vpp->extra_argv[i],len);
    memset((mainw->vpp->extra_argv[i])+len,0,1);
  }

  mainw->vpp->extra_argv[i]=NULL;

  close(fd);
  d_print_done();

}


void on_vppa_cancel_clicked (GtkButton *button, gpointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  _vid_playback_plugin *vpp=vppw->plugin;

  gtk_widget_destroy(vppw->dialog);
  while (g_main_context_iteration (NULL,FALSE));
  if (vpp!=NULL&&vpp!=mainw->vpp) {
    // close the temp current vpp
    close_vid_playback_plugin(vpp);
  }

  if (vppw->rfx!=NULL) {
    rfx_free(vppw->rfx);
    g_free(vppw->rfx);
  }

  g_free(vppw);

  if (prefsw!=NULL) {
    gtk_window_present(GTK_WINDOW(prefsw->prefs_dialog));
    gdk_window_raise(prefsw->prefs_dialog->window);
  }
}


void on_vppa_ok_clicked (GtkButton *button, gpointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  const gchar *fixed_fps=NULL;
  gchar *cur_pal=NULL;
  const gchar *tmp;
  int *pal_list,i=0;

  _vid_playback_plugin *vpp=vppw->plugin;

  if (vpp==mainw->vpp) {
    if (vppw->spinbuttonw!=NULL) mainw->vpp->fwidth=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vppw->spinbuttonw));
    if (vppw->spinbuttonh!=NULL) mainw->vpp->fheight=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vppw->spinbuttonh));
    if (vppw->fps_entry!=NULL) fixed_fps=gtk_entry_get_text(GTK_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry!=NULL) {
      cur_pal=g_strdup(gtk_entry_get_text(GTK_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal,' ')>1) {
	gchar **array=g_strsplit(cur_pal," ",2);
	gchar *clamping=g_strdup(array[1]+1);
	g_free(cur_pal);
	cur_pal=g_strdup(array[0]);
	memset(clamping+strlen(clamping)-1,0,1);
	do {
	  tmp=weed_yuv_clamping_get_name(i);
	  if (tmp!=NULL&&!strcmp(clamping,tmp)) {
	    vpp->YUV_clamping=i;
	    break;
	  }
	  i++;
	} while (tmp!=NULL);
	g_strfreev(array);
	g_free(clamping);
      }
    }

    if (vppw->fps_entry!=NULL) {
      if (!strlen(fixed_fps)) {
	mainw->vpp->fixed_fpsd=-1.;
	mainw->vpp->fixed_fps_numer=0;
      }
      else {
	if (get_token_count ((gchar *)fixed_fps,':')>1) {
	  gchar **array=g_strsplit(fixed_fps,":",2);
	  mainw->vpp->fixed_fps_numer=atoi(array[0]);
	  mainw->vpp->fixed_fps_denom=atoi(array[1]);
	  g_strfreev(array);
	  mainw->vpp->fixed_fpsd=get_ratio_fps((gchar *)fixed_fps);
	}
	else {
	  mainw->vpp->fixed_fpsd=g_strtod(fixed_fps,NULL);
	  mainw->vpp->fixed_fps_numer=0;
	}
      }
    }
    else {
      mainw->vpp->fixed_fpsd=-1.;
      mainw->vpp->fixed_fps_numer=0;
    }

    if (mainw->vpp->fixed_fpsd>0.&&(mainw->fixed_fpsd>0.||!((*mainw->vpp->set_fps) (mainw->vpp->fixed_fpsd)))) {
      do_vpp_fps_error();
      mainw->error=TRUE;
      mainw->vpp->fixed_fpsd=-1.;
      mainw->vpp->fixed_fps_numer=0;
    }

    if (vppw->pal_entry!=NULL) {
      if (vpp->get_palette_list!=NULL&&(pal_list=(*vpp->get_palette_list)())!=NULL) {
	for (i=0;pal_list[i]!=WEED_PALETTE_END;i++) {
	  if (!strcmp(cur_pal,weed_palette_get_name(pal_list[i]))) {
	    vpp->palette=pal_list[i];
	    if (mainw->ext_playback) {
	      if (mainw->vpp->exit_screen!=NULL) {
		(*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
	      }

	      stop_audio_stream();

	      mainw->stream_ticks=-1;
	      mainw->vpp->palette=pal_list[i];
	      if (!(*vpp->set_palette)(vpp->palette)) {
		do_vpp_palette_error();
		mainw->error=TRUE;
	      }

	      if (vpp->set_yuv_palette_clamping!=NULL) (*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

	      if (mainw->vpp->audio_codec!=AUDIO_CODEC_NONE&&prefs->stream_audio_out) {
		start_audio_stream();
	      }

	      if (vpp->init_screen!=NULL) {
		(*vpp->init_screen)(mainw->pwidth,mainw->pheight,TRUE,0,vpp->extra_argc,vpp->extra_argv);
	      }
	    }
	    else {
	      mainw->vpp->palette=pal_list[i];
	      if (!(*vpp->set_palette)(vpp->palette)) {
		do_vpp_palette_error();
		mainw->error=TRUE;
	      }
	      if (vpp->set_yuv_palette_clamping!=NULL) (*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);
	    }
	    break;
	  }
	}
      }
      g_free(cur_pal);
    }
    if (vpp->extra_argv!=NULL) {
      for (i=0;vpp->extra_argv[i]!=NULL;i++) g_free(vpp->extra_argv[i]);
      g_free(vpp->extra_argv);
      vpp->extra_argv=NULL;
    }
    vpp->extra_argc=0;
    if (vppw->rfx!=NULL) {
      vpp->extra_argv=param_marshall_to_argv(vppw->rfx);
      for (i=0;vpp->extra_argv[i]!=NULL;vpp->extra_argc=++i);
    }
    mainw->write_vpp_file=TRUE;
  }
  else {
    if (vppw->spinbuttonw!=NULL) future_prefs->vpp_fwidth=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vppw->spinbuttonw));
    else future_prefs->vpp_fwidth=-1;
    if (vppw->spinbuttonh!=NULL) future_prefs->vpp_fheight=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(vppw->spinbuttonh));
    else future_prefs->vpp_fheight=-1;
    if (vppw->fps_entry!=NULL) fixed_fps=gtk_entry_get_text(GTK_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry!=NULL) {
      cur_pal=g_strdup(gtk_entry_get_text(GTK_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal,' ')>1) {
	gchar **array=g_strsplit(cur_pal," ",2);
	gchar *clamping=g_strdup(array[1]+1);
	g_free(cur_pal);
	cur_pal=g_strdup(array[0]);
	memset(clamping+strlen(clamping)-1,0,1);
	do {
	  tmp=weed_yuv_clamping_get_name(i);
	  if (tmp!=NULL&&!strcmp(clamping,tmp)) {
	    future_prefs->vpp_YUV_clamping=i;
	    break;
	  }
	  i++;
	} while (tmp!=NULL);
	g_strfreev(array);
	g_free(clamping);
      }
    }

    if (fixed_fps!=NULL) {
      if (get_token_count ((gchar *)fixed_fps,':')>1) {
	gchar **array=g_strsplit(fixed_fps,":",2);
	future_prefs->vpp_fixed_fps_numer=atoi(array[0]);
	future_prefs->vpp_fixed_fps_denom=atoi(array[1]);
	g_strfreev(array);
	future_prefs->vpp_fixed_fpsd=get_ratio_fps((gchar *)fixed_fps);
      }
      else {
	future_prefs->vpp_fixed_fpsd=g_strtod(fixed_fps,NULL);
	future_prefs->vpp_fixed_fps_numer=0;
      }
    }
    else {
      future_prefs->vpp_fixed_fpsd=-1.;
      future_prefs->vpp_fixed_fps_numer=0;
    }

    if (cur_pal!=NULL) {
      if (vpp->get_palette_list!=NULL&&(pal_list=(*vpp->get_palette_list)())!=NULL) {
	for (i=0;pal_list[i]!=WEED_PALETTE_END;i++) {
	  if (!strcmp(cur_pal,weed_palette_get_name(pal_list[i]))) {
	    future_prefs->vpp_palette=pal_list[i];
	    break;
	  }
	}
      }
      g_free(cur_pal);
    }
    else future_prefs->vpp_palette=WEED_PALETTE_END;

    if (future_prefs->vpp_argv!=NULL) {
      for (i=0;future_prefs->vpp_argv[i]!=NULL;i++) g_free(future_prefs->vpp_argv[i]);
      g_free(future_prefs->vpp_argv);
      future_prefs->vpp_argv=NULL;
    }

    future_prefs->vpp_argc=0;
    if (vppw->rfx!=NULL) {
      future_prefs->vpp_argv=param_marshall_to_argv(vppw->rfx);
      if (future_prefs->vpp_argv!=NULL) {
	for (i=0;future_prefs->vpp_argv[i]!=NULL;future_prefs->vpp_argc=++i);
      }
    }
    else {
      future_prefs->vpp_argv=vpp->extra_argv;
      vpp->extra_argv=NULL;
      vpp->extra_argc=0;
    }
  }
  if (button!=NULL&&!mainw->error) on_vppa_cancel_clicked(button,user_data);
  if (button!=NULL) mainw->error=FALSE;
}


void on_vppa_save_clicked (GtkButton *button, gpointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  _vid_playback_plugin *vpp=vppw->plugin;
  gchar *save_file;
  gchar *msg;

  // apply
  mainw->error=FALSE;
  on_vppa_ok_clicked(NULL,user_data);
  if (mainw->error) {
    mainw->error=FALSE;
    return;
  }

  // get filename
  save_file=choose_file(NULL,NULL,NULL,GTK_FILE_CHOOSER_ACTION_SAVE,NULL);
  if (save_file==NULL) return;

  // save
  msg=g_strdup_printf(_("Saving playback plugin defaults to %s..."),save_file);
  d_print(msg);
  g_free(msg);
  save_vpp_defaults(vpp, save_file);
  d_print_done();
  g_free(save_file);

}


_vppaw *on_vpp_advanced_clicked (GtkButton *button, gpointer user_data) {
  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkWidget *savebutton;
  GtkObject *spinbutton_adj;

  gchar *title;

  const gchar *pversion;
  const gchar *desc;
  const char *fps_list;
  int *pal_list;
  GList *fps_list_strings=NULL;
  GList *pal_list_strings=NULL;

  const gchar *string;

  gchar *ctext=NULL;

  _vppaw *vppa;

  _vid_playback_plugin *tmpvpp;

  gchar *com;
  
  // TODO - set default values from tmpvpp

  if (strlen(future_prefs->vpp_name)) {
    if ((tmpvpp=open_vid_playback_plugin (future_prefs->vpp_name, FALSE))==NULL) return NULL;
  }
  else {
    tmpvpp=mainw->vpp;
  }

  vppa=(_vppaw*)(g_malloc(sizeof(_vppaw)));

  vppa->plugin=tmpvpp;

  vppa->spinbuttonh=vppa->spinbuttonw=NULL;
  vppa->pal_entry=vppa->fps_entry=NULL;

  vppa->dialog = gtk_dialog_new ();

  gtk_window_set_position (GTK_WINDOW (vppa->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  if (prefs->show_gui) {
    if (prefsw!=NULL) gtk_window_set_transient_for(GTK_WINDOW(vppa->dialog),GTK_WINDOW(prefsw->prefs_dialog));
    else {
      if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(vppa->dialog),GTK_WINDOW(mainw->LiVES));
      else gtk_window_set_transient_for(GTK_WINDOW(vppa->dialog),GTK_WINDOW(mainw->multitrack->window));
    }
  }

  if (prefs->gui_monitor!=0) {
    gtk_window_set_screen(GTK_WINDOW(vppa->dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
  }

  gtk_window_set_modal (GTK_WINDOW (vppa->dialog), TRUE);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(vppa->dialog),FALSE);
    gtk_widget_modify_bg (vppa->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_default_size (GTK_WINDOW (vppa->dialog), 600, 400);

  gtk_container_set_border_width (GTK_CONTAINER (vppa->dialog), 10);

  pversion=(tmpvpp->version)();
  title=g_strdup_printf("LiVES: - %s",pversion);
  gtk_window_set_title (GTK_WINDOW (vppa->dialog), title);
  g_free(title);

  dialog_vbox = GTK_DIALOG (vppa->dialog)->vbox;
  gtk_widget_show (dialog_vbox);


  // the filling...
  if (tmpvpp->get_description!=NULL) {
    desc=(tmpvpp->get_description)();
    if (desc!=NULL) {
      label = gtk_label_new (desc);
      
      if (palette->style&STYLE_1) {
	gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
      }
      
      gtk_box_pack_start (GTK_BOX (dialog_vbox), label, FALSE, FALSE, 10);
    }
  }

  // fps
  combo = gtk_combo_new ();

  if (tmpvpp->get_fps_list!=NULL&&(fps_list=(*tmpvpp->get_fps_list)(tmpvpp->palette))!=NULL) {
    int nfps,i;
    gchar **array=g_strsplit (fps_list,"|",-1);
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));

    gtk_tooltips_set_tip (mainw->tooltips, combo, _("Fixed framerate for plugin.\n"), NULL);
    
    label = gtk_label_new_with_mnemonic (_("_FPS"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),GTK_COMBO(combo)->entry);
  
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));

    vppa->fps_entry=(GTK_COMBO(combo))->entry;

    gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (combo))->entry),FALSE);
    
    nfps=get_token_count((gchar *)fps_list,'|');
    for (i=0;i<nfps;i++) {
      if (strlen(array[i])&&strcmp(array[i],"\n")) {
	if (get_token_count(array[i],':')==0) {
	  fps_list_strings=g_list_append (fps_list_strings, remove_trailing_zeroes(g_strtod(array[i],NULL)));
	}
	else fps_list_strings=g_list_append (fps_list_strings,g_strdup(array[i]));
      }
    }

    combo_set_popdown_strings (GTK_COMBO (combo), fps_list_strings);
    g_list_free_strings(fps_list_strings);
    g_list_free(fps_list_strings);
    fps_list_strings=NULL;
    g_strfreev (array);

    if (tmpvpp->fixed_fps_numer>0) {
      gchar *tmp=g_strdup_printf("%d:%d",tmpvpp->fixed_fps_numer,tmpvpp->fixed_fps_denom);
      gtk_entry_set_text(GTK_ENTRY(vppa->fps_entry),tmp);
      g_free(tmp);
    }
    else {
      gchar *tmp=remove_trailing_zeroes(tmpvpp->fixed_fpsd);
      gtk_entry_set_text(GTK_ENTRY(vppa->fps_entry),tmp);
      g_free(tmp);
    }
  }


  // frame size

  if (!(tmpvpp->capabilities&VPP_LOCAL_DISPLAY)) {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    
    add_fill_to_box(GTK_BOX(hbox));
    label=gtk_label_new_with_mnemonic(_("_Width"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (tmpvpp->fwidth>0?tmpvpp->fwidth:DEF_VPP_HSIZE, 4., 100000, 4, 4, 0.);
    vppa->spinbuttonw = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 4, 0);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label),vppa->spinbuttonw);
    gtk_box_pack_start (GTK_BOX (hbox), vppa->spinbuttonw, FALSE, FALSE, 10);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (vppa->spinbuttonw),GTK_UPDATE_IF_VALID);
    
    add_fill_to_box(GTK_BOX(hbox));
    
    label=gtk_label_new_with_mnemonic(_("_Height"));
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    
    spinbutton_adj = gtk_adjustment_new (tmpvpp->fheight>0?tmpvpp->fheight:DEF_VPP_VSIZE, 4., 100000, 4, 4, 0.);
    vppa->spinbuttonh = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 4, 0);

    gtk_label_set_mnemonic_widget (GTK_LABEL (label),vppa->spinbuttonh);
    gtk_box_pack_start (GTK_BOX (hbox), vppa->spinbuttonh, FALSE, FALSE, 10);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (vppa->spinbuttonh),GTK_UPDATE_IF_VALID);
    add_fill_to_box(GTK_BOX(hbox));
  }

  
  // palette

  combo = gtk_combo_new ();
  if (tmpvpp->get_palette_list!=NULL&&(pal_list=(*tmpvpp->get_palette_list)())!=NULL) {
    int i;
    
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));
    
    gtk_tooltips_set_tip (mainw->tooltips, combo, _("Colourspace input to the plugin.\n"), NULL);
    
    label = gtk_label_new_with_mnemonic (_("_Colourspace"));
    gtk_label_set_mnemonic_widget (GTK_LABEL (label),GTK_COMBO(combo)->entry);
  
    if (palette->style&STYLE_1) {
      gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    }
    
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 10);
    gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 10);
    add_fill_to_box(GTK_BOX(hbox));

    vppa->pal_entry=(GTK_COMBO(combo))->entry;

    gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO (combo))->entry),FALSE);
    gtk_entry_set_activates_default(GTK_ENTRY((GTK_COMBO(combo))->entry),TRUE);
    
    for (i=0;pal_list[i]!=WEED_PALETTE_END;i++) {
      int j=0;
      string=weed_palette_get_name(pal_list[i]);
      if (weed_palette_is_yuv_palette(pal_list[i])&&tmpvpp->get_yuv_palette_clamping!=NULL) {
       	int *clampings=(*tmpvpp->get_yuv_palette_clamping)(pal_list[i]);
	while (clampings[j]!=-1) {
	  gchar *string2=g_strdup_printf("%s (%s)",string,weed_yuv_clamping_get_name(clampings[j]));
	  pal_list_strings=g_list_append (pal_list_strings, string2);
	  j++;
	}
      }
      if (j==0) {
	pal_list_strings=g_list_append (pal_list_strings, g_strdup(string));
      }
    }
    combo_set_popdown_strings (GTK_COMBO (combo), pal_list_strings);

    if (tmpvpp->get_yuv_palette_clamping!=NULL&&weed_palette_is_yuv_palette(tmpvpp->palette)) {
      int *clampings=tmpvpp->get_yuv_palette_clamping(tmpvpp->palette);
      if (clampings[0]!=-1) ctext=g_strdup_printf("%s (%s)",weed_palette_get_name(tmpvpp->palette),weed_yuv_clamping_get_name(tmpvpp->YUV_clamping));
    }
    if (ctext==NULL) ctext=g_strdup(weed_palette_get_name(tmpvpp->palette));
    gtk_entry_set_text(GTK_ENTRY(vppa->pal_entry),ctext);
    g_free(ctext);
    g_list_free_strings(pal_list_strings);
    g_list_free(pal_list_strings);
  }

  // extra params

  if (tmpvpp->get_rfx!=NULL) {
    com=g_strdup_printf("echo -e \"%s\"",(*tmpvpp->get_rfx)());
    plugin_run_param_window(com,GTK_VBOX(dialog_vbox),&(vppa->rfx));
    g_free(com);
    if (tmpvpp->extra_argv!=NULL&&tmpvpp->extra_argc>0) {
      // update with defaults
      GList *plist=argv_to_marshalled_list(vppa->rfx,tmpvpp->extra_argc,tmpvpp->extra_argv);
      param_demarshall(vppa->rfx,plist,FALSE,TRUE);
      g_list_free_strings(plist);
      g_list_free(plist);
    }
  }
  else {
    vppa->rfx=NULL;
  }


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_widget_show (cancelbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (vppa->dialog), cancelbutton, GTK_RESPONSE_CANCEL);

  savebutton = gtk_button_new_from_stock ("gtk-save-as");
  gtk_widget_show (savebutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (vppa->dialog), savebutton, 1);
  GTK_WIDGET_SET_FLAGS (savebutton, GTK_CAN_DEFAULT);
  gtk_tooltips_set_tip (mainw->tooltips, savebutton, _("Save settings to an alternate file.\n"), NULL);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_widget_show (okbutton);
  gtk_dialog_add_action_widget (GTK_DIALOG (vppa->dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);

  g_signal_connect (GTK_OBJECT (cancelbutton), "clicked",
		    G_CALLBACK (on_vppa_cancel_clicked),
		    vppa);

  g_signal_connect (GTK_OBJECT (savebutton), "clicked",
		    G_CALLBACK (on_vppa_save_clicked),
		    vppa);

  g_signal_connect (GTK_OBJECT (okbutton), "clicked",
		    G_CALLBACK (on_vppa_ok_clicked),
		    vppa);


  gtk_widget_show_all(vppa->dialog);
  gtk_window_present (GTK_WINDOW (vppa->dialog));
  gdk_window_raise(vppa->dialog->window);

  return vppa;
}


void close_vid_playback_plugin(_vid_playback_plugin *vpp) {
  int i;

  if (vpp!=NULL) {
    if (vpp==mainw->vpp) {
      if (mainw->ext_playback&&mainw->vpp->exit_screen!=NULL) (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
      stop_audio_stream();
      mainw->stream_ticks=-1;
      mainw->vpp=NULL;
    }
    if (vpp->module_unload!=NULL) (vpp->module_unload)();
    dlclose (vpp->handle);

    if (vpp->extra_argv!=NULL) {
      for (i=0;vpp->extra_argv[i]!=NULL;i++) {
	g_free(vpp->extra_argv[i]);
      }
      g_free(vpp->extra_argv);
    }

    g_free (vpp);
  }
}


_vid_playback_plugin *open_vid_playback_plugin (const gchar *name, gboolean using) {
  // this is called on startup or when the user selects a new playback plugin

  // if using is TRUE, it is our active vpp

  // TODO - if using, get fixed_fps,fwidth,fheight,palette,argc and argv from a file

  gchar *plugname=g_strdup_printf ("%s%s%s/%s.so",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_VID_PLAYBACK,name);
  void *handle=dlopen(plugname,RTLD_LAZY);
  gboolean OK=TRUE;
  gchar *msg,*tmp;
  gchar **array;
  const gchar *fps_list;
  const gchar *pl_error;
  int i;
  int *palette_list;
  _vid_playback_plugin *vpp;
  gboolean needs_restart=FALSE;

  if (handle==NULL) {
    gchar *msg=g_strdup_printf (_("\n\nFailed to open playback plugin %s\nError was %s\n"),plugname,dlerror());
    if (prefsw!=NULL) do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
    else do_error_dialog(msg);
    g_free (msg);
    g_free(plugname);
    return NULL;
  }

  vpp=(_vid_playback_plugin *) g_malloc (sizeof(_vid_playback_plugin));

  if ((vpp->module_check_init=dlsym (handle,"module_check_init"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->version=dlsym (handle,"version"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_palette_list=dlsym (handle,"get_palette_list"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->set_palette=dlsym (handle,"set_palette"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_capabilities=dlsym (handle,"get_capabilities"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->render_frame=dlsym (handle,"render_frame"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_fps_list=dlsym (handle,"get_fps_list"))!=NULL) {
    if ((vpp->set_fps=dlsym (handle,"set_fps"))==NULL) {
      OK=FALSE;
    }
  }

  if ((pl_error=(*vpp->module_check_init)())!=NULL) {
    msg=g_strdup_printf(_("Video playback plugin failed to initialise.\nError was: %s\n"),pl_error);
    do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
    g_free(msg);
    dlclose (handle);
    g_free (vpp);
    vpp=NULL;
    g_free(plugname);
    return NULL;
  }

  if (!OK) {
    gchar *msg=g_strdup_printf (_("\n\nPlayback module %s\nis missing a mandatory function.\nUnable to use it.\n"),plugname);
    set_pref ("vid_playback_plugin","none");
    do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?GTK_WINDOW(prefsw->prefs_dialog):GTK_WINDOW(mainw->LiVES->window));
    g_free (msg);
    close_vid_playback_plugin(vpp);
    g_free(plugname);
    return NULL;
  }

  // now check for optional functions
  vpp->get_description=dlsym (handle,"get_description");
  vpp->get_rfx=dlsym (handle,"get_rfx");
  vpp->get_yuv_palette_clamping=dlsym (handle,"get_yuv_palette_clamping");
  vpp->set_yuv_palette_clamping=dlsym (handle,"set_yuv_palette_clamping");
  vpp->send_keycodes=dlsym (handle,"send_keycodes");
  vpp->get_audio_fmts=dlsym (handle,"get_audio_fmts");
  vpp->init_screen=dlsym (handle,"init_screen");
  vpp->exit_screen=dlsym (handle,"exit_screen");
  vpp->module_unload=dlsym (handle,"module_unload");

  vpp->YUV_sampling=0;
  vpp->YUV_subspace=0;

  palette_list=(*vpp->get_palette_list)();

  if (future_prefs->vpp_argv!=NULL) {
    vpp->palette=future_prefs->vpp_palette;
    vpp->YUV_clamping=future_prefs->vpp_YUV_clamping;
  }
  else {
    if (!using&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
      vpp->palette=mainw->vpp->palette;
      vpp->YUV_clamping=mainw->vpp->YUV_clamping;
    }
    else {
      vpp->palette=palette_list[0];
      vpp->YUV_clamping=-1;
    }
  }

  vpp->audio_codec=AUDIO_CODEC_NONE;
  vpp->capabilities=(*vpp->get_capabilities)(vpp->palette);

  if (vpp->capabilities&VPP_CAN_RESIZE) {
    vpp->fwidth=vpp->fheight=-1;
  }
  else {
    vpp->fwidth=vpp->fheight=0;
  }
  if (future_prefs->vpp_argv!=NULL) {
    vpp->fwidth=future_prefs->vpp_fwidth;
    vpp->fheight=future_prefs->vpp_fheight;
  }
  else if (!using&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
    vpp->fwidth=mainw->vpp->fwidth;
    vpp->fheight=mainw->vpp->fheight;
  }

  vpp->fixed_fpsd=-1.;
  vpp->fixed_fps_numer=0;

  if (future_prefs->vpp_argv!=NULL) {
    vpp->fixed_fpsd=future_prefs->vpp_fixed_fpsd;
    vpp->fixed_fps_numer=future_prefs->vpp_fixed_fps_numer;
    vpp->fixed_fps_denom=future_prefs->vpp_fixed_fps_denom;
  }
  else if (!using&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
    vpp->fixed_fpsd=mainw->vpp->fixed_fpsd;
    vpp->fixed_fps_numer=mainw->vpp->fixed_fps_numer;
    vpp->fixed_fps_denom=mainw->vpp->fixed_fps_denom;
  }

  vpp->handle=handle;
  g_snprintf (vpp->name,256,"%s",name);

  if (future_prefs->vpp_argv!=NULL) {
    vpp->extra_argc=future_prefs->vpp_argc;
    vpp->extra_argv=g_malloc((vpp->extra_argc+1)*(sizeof(gchar *)));
    for (i=0;i<=vpp->extra_argc;i++) vpp->extra_argv[i]=g_strdup(future_prefs->vpp_argv[i]);
  }
  else {
    if (!using&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
    vpp->extra_argc=mainw->vpp->extra_argc;
    vpp->extra_argv=g_malloc((mainw->vpp->extra_argc+1)*(sizeof(gchar *)));
    for (i=0;i<=vpp->extra_argc;i++) vpp->extra_argv[i]=g_strdup(mainw->vpp->extra_argv[i]);
    }
    else {
      vpp->extra_argc=0;
      vpp->extra_argv=(gchar **)g_malloc(sizeof(gchar *));
      vpp->extra_argv[0]=NULL;
    }
  }
  // see if plugin is using fixed fps

  if (vpp->fixed_fpsd<=0.&&vpp->get_fps_list!=NULL) {
    // fixed fps

    if ((fps_list=(*vpp->get_fps_list)(vpp->palette))!=NULL) {
      array=g_strsplit (fps_list,"|",-1);
      if (get_token_count (array[0],':')>1) {
	gchar **array2=g_strsplit(array[0],":",2);
	vpp->fixed_fps_numer=atoi(array2[0]);
	vpp->fixed_fps_denom=atoi(array2[1]);
	g_strfreev(array2);
	vpp->fixed_fpsd=get_ratio_fps(array[0]);
      }
      else {
	vpp->fixed_fpsd=g_strtod(array[0],NULL);
	vpp->fixed_fps_numer=0;
      }
      g_strfreev(array);
    }
  }


  if (vpp->YUV_clamping==-1) {
    vpp->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;
    
    if (vpp->get_yuv_palette_clamping!=NULL&&weed_palette_is_yuv_palette(vpp->palette)) {
      int *yuv_clamping_types=(*vpp->get_yuv_palette_clamping)(vpp->palette);
      if (yuv_clamping_types[0]!=-1) vpp->YUV_clamping=yuv_clamping_types[0];
    }
  }

  if (vpp->get_audio_fmts!=NULL&&mainw->is_ready) vpp->audio_codec=get_best_audio(vpp);

  if (prefsw!=NULL) prefsw_set_astream_settings(vpp);

  if (!using) return vpp;

  if (!mainw->is_ready) {
    double fixed_fpsd=vpp->fixed_fpsd;
    gint fwidth=vpp->fwidth;
    gint fheight=vpp->fheight;

    mainw->vpp=vpp;
    load_vpp_defaults(vpp, mainw->vpp_defs_file);
    if (fixed_fpsd<0.) vpp->fixed_fpsd=fixed_fpsd;
    if (fwidth<0) vpp->fwidth=fwidth;
    if (fheight<0) vpp->fheight=fheight;
  }

  if (!(*vpp->set_palette)(vpp->palette)) {
    do_vpp_palette_error();
    close_vid_playback_plugin(vpp);
    g_free(plugname);
    return NULL;
  }
    
  if (vpp->set_yuv_palette_clamping!=NULL) (*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

  if (vpp->get_fps_list!=NULL) {
    if (mainw->fixed_fpsd>0.||(vpp->fixed_fpsd>0.&&!((*vpp->set_fps) (vpp->fixed_fpsd)))) {
      do_vpp_fps_error();
      vpp->fixed_fpsd=-1.;
      vpp->fixed_fps_numer=0;
    }
  }


  if (vpp->send_keycodes==NULL&&vpp->capabilities&VPP_LOCAL_DISPLAY) {
    d_print(_("\nWarning ! Video playback plugin will not send key presses. Keyboard may be disabled during plugin use !\n"));
  }

  cached_key=cached_mod=0;
  msg=g_strdup_printf(_("*** Using %s plugin for fs playback, agreed to use palette type %d ( %s ). ***\n"),name,vpp->palette,(tmp=weed_palette_get_name_full(vpp->palette,vpp->YUV_clamping,WEED_YUV_SUBSPACE_YCBCR)));
  g_free(tmp);
  d_print (msg);
  g_free (msg);
  g_free(plugname);

  if (mainw->ext_playback) needs_restart=TRUE;

  while (mainw->noswitch) {
    g_usleep(prefs->sleep_time);
  }

  if (mainw->is_ready&&using&&mainw->vpp!=NULL) {
    close_vid_playback_plugin(mainw->vpp);
  }
  
  return vpp;
}


void vid_playback_plugin_exit (void) {
  // external plugin
  if (mainw->ext_playback) {
    if (mainw->vpp->exit_screen!=NULL) (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
    stop_audio_stream();
    mainw->ext_playback=FALSE;
  }
  mainw->stream_ticks=-1;
  mainw->ext_keyboard=FALSE;

  if (mainw->playing_file>-1&&mainw->fs&&mainw->sep_win) gtk_window_fullscreen(GTK_WINDOW(mainw->play_window));
  gtk_window_set_title (GTK_WINDOW (mainw->play_window),_("LiVES: - Play Window"));
}


gint64 get_best_audio(_vid_playback_plugin *vpp) {
  // find best audio from video plugin list, matching with audiostream plugins
  int *fmts,*sfmts;
  int ret=AUDIO_CODEC_NONE;
  int i,j=0,nfmts;
  size_t rlen;
  gchar *astreamer,*com,*playername;
  gchar buf[1024];
  gchar **array;
  FILE *rfile;

  if (vpp!=NULL&&vpp->get_audio_fmts!=NULL) {
    fmts=(*vpp->get_audio_fmts)();

    // make audiostream plugin name
    playername=g_strdup_printf("%sstreamer.pl",prefs->aplayer);
    astreamer=g_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,playername,NULL);
    g_free(playername);

    // create sfmts array and nfmts

    com=g_strdup_printf("%s get_formats",astreamer);
    g_free(astreamer);

    rfile=popen(com,"r");
    rlen=fread(buf,1,1023,rfile);
    pclose(rfile);
    memset(buf+rlen,0,1);
    g_free(com);

    nfmts=get_token_count(buf,'|');
    array=g_strsplit(buf,"|",nfmts);
    sfmts=g_malloc(nfmts*sizint);

    for (i=0;i<nfmts;i++) {
      if (array[i]!=NULL&&strlen(array[i])>0) sfmts[j++]=atoi(array[i]);
    }

    nfmts=j;
    g_strfreev(array);

    for (i=0;fmts[i]!=-1;i++) {
      // traverse video list and see if audiostreamer supports each one
      if (int_array_contains_value(sfmts,nfmts,fmts[i])) {
	ret=fmts[i];
	break;
      }
    }
    g_free(sfmts);
  }

  return ret;
}


///////////////////////
// encoder plugins



void 
do_plugin_encoder_error(const gchar *plugin_name) {
  gchar *msg;

  if (plugin_name==NULL) {
    msg=g_strdup_printf(_("LiVES was unable to find its encoder plugins. Please make sure you have the plugins installed in\n%s%s%s\nor change the value of <lib_dir> in ~/.lives\n"),prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS);
    if (rdet!=NULL) do_error_dialog_with_check_transient(msg,FALSE,0,GTK_WINDOW(rdet->dialog));
    else do_error_dialog(msg);
    g_free(msg);
    return;
  }

  msg=g_strdup_printf(_("LiVES did not receive a response from the encoder plugin called '%s'.\nPlease make sure you have that plugin installed correctly in\n%s%s%s\nor switch to another plugin using Tools|Preferences|Encoding\n"),plugin_name,prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS);
  do_blocking_error_dialog(msg);
  g_free(msg);
}


gboolean check_encoder_restrictions (gboolean get_extension, gboolean user_audio) {
  gchar **checks;
  gchar **array=NULL;
  gchar **array2;
  gint pieces,numtok;
  gboolean calc_aspect=FALSE;
  gchar aspect_buffer[512];
  gint hblock=2,vblock=2;
  int i,r,val;
  GList *ofmt_all=NULL;
  gboolean sizer=FALSE;

  // for auto resizing/resampling
  gdouble best_fps=0.;
  gint best_arate=0;
  gint width,owidth;
  gint height,oheight;

  gdouble best_fps_delta=0.;
  gint best_arate_delta=0;
  gboolean allow_aspect_override=FALSE;

  gint best_fps_num=0,best_fps_denom=0;
  gdouble fps;
  gint arate,achans,asampsize,asigned=0;

  if (rdet==NULL) {
    width=owidth=cfile->hsize;
    height=oheight=cfile->vsize;
    fps=cfile->fps;
  }
  else {
    width=owidth=rdet->width;
    height=oheight=rdet->height;
    fps=rdet->fps;
    rdet->suggestion_followed=FALSE;
  }


  if (mainw->osc_auto&&mainw->osc_enc_width>0) {
    width=mainw->osc_enc_width;
    height=mainw->osc_enc_height;
  }

  // TODO - allow lists for size
  g_snprintf (prefs->encoder.of_restrict,5,"none");
  if (!((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))==NULL)) {
    // get any restrictions for the current format
    for (i=0;i<g_list_length(ofmt_all);i++) {
      if ((numtok=get_token_count (g_list_nth_data (ofmt_all,i),'|'))>2) {
	array=g_strsplit (g_list_nth_data (ofmt_all,i),"|",-1);
	if (!strcmp(array[0],prefs->encoder.of_name)) {
	  if (numtok>4) {
	    g_snprintf(prefs->encoder.of_def_ext,16,"%s",array[4]);
	  }
	  else {
	    memset (prefs->encoder.of_def_ext,0,1);
	  }
	  if (numtok>3) {
	    g_snprintf(prefs->encoder.of_restrict,128,"%s",array[3]);
	  }
	  else {
	    g_snprintf(prefs->encoder.of_restrict,128,"none");
	  }
	  prefs->encoder.of_allowed_acodecs=atoi (array[2]);
	  g_list_free_strings (ofmt_all);
	  g_list_free (ofmt_all);
	  g_strfreev(array);
	  break;
	}
	g_strfreev(array);
      }
    }
  }

  if (get_extension) {
    return TRUE; // just wanted file extension
  }

  if (rdet==NULL&&mainw->save_with_sound&&prefs->encoder.audio_codec!=AUDIO_CODEC_NONE) {
    if (!(prefs->encoder.of_allowed_acodecs&(1<<prefs->encoder.audio_codec))) {
      do_encoder_acodec_error();
      return FALSE;
    }
  }

  if (user_audio&&future_prefs->encoder.of_allowed_acodecs==0) best_arate=-1;

  if ((strlen(prefs->encoder.of_restrict)==0||!strcmp(prefs->encoder.of_restrict,"none"))&&best_arate>-1) {
    return TRUE;
  }

  if (rdet==NULL) {
    arate=cfile->arate;
    achans=cfile->achans;
    asampsize=cfile->asampsize;
  }
  else {
    arate=rdet->arate;
    achans=rdet->achans;
    asampsize=rdet->asamps;
  }







  if (strlen(prefs->encoder.of_restrict)>0) {

    pieces=get_token_count (prefs->encoder.of_restrict,',');
    checks=g_strsplit(prefs->encoder.of_restrict,",",pieces);
    

    for (r=0;r<pieces;r++) {
      // check each restriction in turn
      
      if (!strncmp (checks[r],"fps=",4)) {
	gdouble allowed_fps;
	gint mbest_num=0,mbest_denom=0;
	gint numparts;
	gchar *fixer;
	
	best_fps_delta=1000000000.;
	array=g_strsplit(checks[r],"=",2);
	numtok=get_token_count (array[1],';');
	array2=g_strsplit(array[1],";",numtok);
	for (i=0;i<numtok;i++) {
	  mbest_num=mbest_denom=0;
	  if ((numparts=get_token_count (array2[i],':'))>1) {
	    gchar **array3=g_strsplit(array2[i],":",2);
	    mbest_num=atoi (array3[0]);
	    mbest_denom=atoi (array3[1]);
	    g_strfreev (array3);
	    if (mbest_denom==0) continue;
	    allowed_fps=(mbest_num*1.)/(mbest_denom*1.);
	  }
	  else allowed_fps=g_strtod (array2[i],NULL);
	  
	  // convert to 8dp
	  fixer=g_strdup_printf ("%.8f %.8f",allowed_fps,fps);
	  g_free (fixer);
	  
	  if (allowed_fps>=fps) {
	    if (allowed_fps-fps<best_fps_delta) {
	      best_fps_delta=allowed_fps-fps;
	      if (mbest_denom>0) {
		best_fps_num=mbest_num;
		best_fps_denom=mbest_denom;
		best_fps=0.;
		if (rdet==NULL) cfile->ratio_fps=TRUE;
		else rdet->ratio_fps=TRUE;
	      }
	      else {
		best_fps_num=best_fps_denom=0;
		best_fps=allowed_fps;
		if (rdet==NULL) cfile->ratio_fps=FALSE;
		else rdet->ratio_fps=FALSE;
	      }
	    }
	  }
	  else if ((best_fps_denom==0&&allowed_fps>best_fps)||(best_fps_denom>0&&allowed_fps>(best_fps_num*1.)/(best_fps_denom*1.))) {
	    best_fps_delta=fps-allowed_fps;
	    if (mbest_denom>0) {
	      best_fps_num=mbest_num;
	      best_fps_denom=mbest_denom;
	      best_fps=0.;
	      if (rdet==NULL) cfile->ratio_fps=TRUE;
	      else rdet->ratio_fps=TRUE;
	    }
	    else {
	      best_fps=allowed_fps;
	      best_fps_num=best_fps_denom=0;
	      if (rdet==NULL) cfile->ratio_fps=FALSE;
	      else rdet->ratio_fps=FALSE;
	    }
	  }
	  if (best_fps_delta<(.0005*prefs->ignore_tiny_fps_diffs)) {
	    best_fps_delta=0.;
	    best_fps_denom=best_fps_num=0;
	  }
	  if (best_fps_delta==0.) break;
	}
	g_strfreev(array);
	g_strfreev(array2);
	continue;
      }
      
      if (!strncmp (checks[r],"size=",5)) {
	// TODO - allow list for size
	array=g_strsplit(checks[r],"=",2);
	array2=g_strsplit(array[1],"x",2);
	width=atoi (array2[0]);
	height=atoi (array2[1]);
	g_strfreev(array2);
	g_strfreev(array);
	sizer=TRUE;
	continue;
      }
      
      if (!strncmp (checks[r],"minw=",5)) {
	array=g_strsplit(checks[r],"=",2);
	val=atoi(array[1]);
	if (width<val) width=val;
	g_strfreev(array);
	continue;
      }

      if (!strncmp (checks[r],"minh=",5)) {
	array=g_strsplit(checks[r],"=",2);
	val=atoi(array[1]);
	if (height<val) height=val;
	g_strfreev(array);
	continue;
      }

      if (!strncmp (checks[r],"maxh=",5)) {
	array=g_strsplit(checks[r],"=",2);
	val=atoi(array[1]);
	if (height>val) height=val;
	g_strfreev(array);
	continue;
      }

      if (!strncmp (checks[r],"maxw=",5)) {
	array=g_strsplit(checks[r],"=",2);
	val=atoi(array[1]);
	if (width>val) width=val;
	g_strfreev(array);
	continue;
      }

      
      if (!strncmp (checks[r],"asigned=",8)&&((mainw->save_with_sound||rdet!=NULL)&&(resaudw==NULL||resaudw->aud_checkbutton==NULL||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton))))&&prefs->encoder.audio_codec!=AUDIO_CODEC_NONE&&(arate*achans*asampsize)) {
	array=g_strsplit(checks[r],"=",2);
	if (!strcmp(array[1],"signed")) {
	  asigned=1;
	}
	
	if (!strcmp(array[1],"unsigned")) {
	  asigned=2;
	}
	
	g_strfreev(array);
      
	if (asigned!=0&&!capable->has_sox) {
	  do_encoder_sox_error();
	  g_strfreev(checks);
	  return FALSE;
	}
	continue;
      }
      
      
      if (!strncmp (checks[r],"arate=",6)&&((mainw->save_with_sound||rdet!=NULL)&&(resaudw==NULL||resaudw->aud_checkbutton==NULL||gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton))))&&prefs->encoder.audio_codec!=AUDIO_CODEC_NONE&&(arate*achans*asampsize)) {
	// we only perform this test if we are encoding with audio
	// find next highest allowed rate from list,
	// if none are higher, use the highest
	gint allowed_arate;
	best_arate_delta=1000000000;
	
	array=g_strsplit(checks[r],"=",2);
	numtok=get_token_count (array[1],';');
	array2=g_strsplit(array[1],";",numtok);
	for (i=0;i<numtok;i++) {
	  allowed_arate=atoi (array2[i]);
	  if (allowed_arate>=arate) {
	    if (allowed_arate-arate<best_arate_delta) {
	      best_arate_delta=allowed_arate-arate;
	      best_arate=allowed_arate;
	    }
	  }
	  else if (allowed_arate>best_arate) best_arate=allowed_arate;
	}
	g_strfreev(array2);
	g_strfreev(array);
	
	if (!capable->has_sox) {
	  do_encoder_sox_error();
	  g_strfreev(checks);
	  return FALSE;
	}
	continue;
      }
      
      if (!strncmp (checks[r],"hblock=",7)) {
	// width must be a multiple of this
	array=g_strsplit(checks[r],"=",2);
	hblock=atoi (array[1]);
	width=(gint)(width/hblock+.5)*hblock;
	g_strfreev(array);
	continue;
      }
      
      if (!strncmp (checks[r],"vblock=",7)) {
	// height must be a multiple of this
	array=g_strsplit(checks[r],"=",2);
	vblock=atoi (array[1]);
	height=(gint)(height/vblock+.5)*vblock;
	g_strfreev(array);
	continue;
      }
      
      if (!strncmp (checks[r],"aspect=",7)) {
	// we calculate the nearest smaller frame size using aspect, 
	// hblock and vblock
	calc_aspect=TRUE;
	array=g_strsplit(checks[r],"=",2);
	g_snprintf (aspect_buffer,512,"%s",array[1]);
	g_strfreev(array);
	continue;
      }
    }
    
    /// end restrictions
    g_strfreev(checks);
    
    if (!mainw->osc_auto&&calc_aspect&&!sizer) {
      // we calculate this last, after getting hblock and vblock sizes
      gchar **array3;
      gdouble allowed_aspect;
      gint xwidth=width;
      gint xheight=height;
      
      width=height=1000000;
      
      numtok=get_token_count (aspect_buffer,';');
      array2=g_strsplit(aspect_buffer,";",numtok);
    
      // see if we can get a width:height which is nearer an aspect than 
      // current width:height
      
      for (i=0;i<numtok;i++) {
	array3=g_strsplit(array2[i],":",2);
	allowed_aspect=g_strtod(array3[0],NULL)/g_strtod(array3[1],NULL);
	g_strfreev(array3);
	minimise_aspect_delta (allowed_aspect,hblock,vblock,xwidth,xheight,&width,&height);
      }
      g_strfreev(array2);
      
      // allow override if current width and height are integer multiples of blocks
      if (owidth%hblock==0&&oheight%vblock==0) allow_aspect_override=TRUE;
      
      // end recheck
    }
    
    // fps can't be altered if we have a multitrack event_list
    if (mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL) best_fps_delta=0.;
    
    if (sizer) allow_aspect_override=FALSE;
    
  }



  // if we have min or max size, make sure we fit within that









  if (((width!=owidth||height!=oheight)&&width*height>0)||(best_fps_delta>0.)||(best_arate_delta>0&&best_arate>0)||best_arate<0||asigned!=0) {
    gboolean ofx1_bool=mainw->fx1_bool;
    mainw->fx1_bool=FALSE;
    if ((width!=owidth||height!=oheight)&&width*height>0) {
      if (!capable->has_convert&&rdet==NULL&&mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1) {
	if (allow_aspect_override) {
	  width=owidth;
	  height=oheight;
	}
	else {
	  do_error_dialog (_ ("Unable to resize, please install imageMagick\n"));
	  return FALSE;
	}
      }
    }
    if (rdet!=NULL&&!rdet->is_encoding) {
      rdet->arate=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_arate)));
      rdet->achans=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_achans)));
      rdet->asamps=(gint)atoi (gtk_entry_get_text(GTK_ENTRY(resaudw->entry_asamps)));
      rdet->aendian=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned))?AFORM_UNSIGNED:AFORM_SIGNED;

      if (width!=rdet->width||height!=rdet->height||best_fps_delta!=0.||best_arate!=rdet->arate||((asigned==1&&rdet->aendian==AFORM_UNSIGNED)||(asigned==2&&rdet->aendian==AFORM_SIGNED))) {
	if (rdet_suggest_values(width,height,best_fps,best_fps_num,best_fps_denom,best_arate,asigned,allow_aspect_override,(best_fps_delta==0.))) {
	  gchar *arate_string;
	  rdet->width=width;
	  rdet->height=height;
	  if (best_arate!=-1) rdet->arate=best_arate;
	  else gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resaudw->aud_checkbutton),FALSE);

	  if (asigned==1) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resaudw->rb_signed),TRUE);
	  else if (asigned==2) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resaudw->rb_unsigned),TRUE);

	  if (best_fps_delta>0.) {
	    if (best_fps_denom>0) {
	      rdet->fps=(best_fps_num*1.)/(best_fps_denom*1.);
	    }
	    else rdet->fps=best_fps;
	    gtk_spin_button_set_value(GTK_SPIN_BUTTON(rdet->spinbutton_fps),rdet->fps);
	  }
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rdet->spinbutton_width),rdet->width);
	  gtk_spin_button_set_value(GTK_SPIN_BUTTON(rdet->spinbutton_height),rdet->height);
	  if (best_arate!=-1) {
	    arate_string=g_strdup_printf("%d",best_arate);
	    gtk_entry_set_text (GTK_ENTRY (resaudw->entry_arate),arate_string);
	    g_free(arate_string);
	  }
	  rdet->suggestion_followed=TRUE;
	  return TRUE;
	}
      }
      return FALSE;
    }

    if (mainw->osc_auto||do_encoder_restrict_dialog (width,height,best_fps,best_fps_num,best_fps_denom,best_arate,asigned,allow_aspect_override)) {
      if (!mainw->fx1_bool&&mainw->osc_enc_width==0) {
	width=owidth;
	height=oheight;
      }

      if (!auto_resample_resize (width,height,best_fps,best_fps_num,best_fps_denom,best_arate,asigned)) {
	mainw->fx1_bool=ofx1_bool;
	return FALSE;
      }
    }
    else {
      mainw->fx1_bool=ofx1_bool;
      return FALSE;
    }
  }
  return TRUE;
}




GList *filter_encoders_by_img_ext(GList *encoders, const gchar *img_ext) {
  GList *encoder_capabilities=NULL;
  GList *list=encoders,*listnext;
  int caps;
  char *blacklist[]={
    NULL
  };

  if (!strcmp(img_ext,"jpg")) return encoders; // jpeg is the default

  while (list!=NULL) {
    gboolean skip=FALSE;
    int i=0;

    listnext=list->next;

    while (blacklist[i++]!=NULL) {
      if (strlen(list->data)==strlen(blacklist[i])&&!strcmp(list->data,blacklist[i])) {
	// skip blacklisted encoders
	g_free(list->data);
	encoders=g_list_delete_link(encoders,list);
	skip=TRUE;
	break;
      }
      i++;
    }
    if (skip) {
      list=listnext;
      continue;
    }

    if ((encoder_capabilities=plugin_request(PLUGIN_ENCODERS,list->data,"get_capabilities"))==NULL) {
      g_free(list->data);
      encoders=g_list_delete_link(encoders,list);
    }
    else {
      caps=atoi (g_list_nth_data (encoder_capabilities,0));
      
      if (!(caps&CAN_ENCODE_PNG)&&!strcmp(img_ext,"png")) {
	g_free(list->data);
	encoders=g_list_delete_link(encoders,list);
      }

      g_list_free_strings (encoder_capabilities);
      g_list_free (encoder_capabilities);

    }

    list=listnext;
  }

  return encoders;

}



//////////////////////////////////////////////////////
// decoder plugins


LIVES_INLINE gboolean decplugin_supports_palette (const lives_decoder_t *dplug, int palette) {
  register int i=0;
  int cpal;
  while ((cpal=dplug->cdata->palettes[i++])!=WEED_PALETTE_END) if (cpal==palette) return TRUE;
  return FALSE;
}



GList *load_decoders(void) {
  lives_decoder_sys_t *dplug;
  gchar *decplugdir=g_strdup_printf("%s%s%s",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS);
  GList *dlist=NULL;
  GList *decoder_plugins_o=get_plugin_list (PLUGIN_DECODERS,TRUE,decplugdir,"-so"),*decoder_plugins=decoder_plugins_o;
  g_free(decplugdir);

  while (decoder_plugins!=NULL) {
    dplug=open_decoder_plugin((gchar *)decoder_plugins->data);
    g_free(decoder_plugins->data);
    if (dplug!=NULL) dlist=g_list_append(dlist,(gpointer)dplug);
    decoder_plugins=decoder_plugins->next;
  }

  g_list_free(decoder_plugins_o);

  return dlist;
}













const lives_clip_data_t *get_decoder_cdata(file *sfile) {
  // pass file to each decoder (demuxer) plugin in turn, until we find one that can parse
  // the file
  // NULL is returned if no decoder plugin recognises the file - then we
  // fall back to other methods

  // otherwise we return data for the clip as supplied by the decoder plugin

  // If the file does not exist, we set mainw->error=TRUE and return NULL

  // If we find a plugin we also set sfile->ext_src to point to a newly created decoder_plugin_t


  gchar *tmp=NULL;
  GList *decoder_plugin;
  gchar *msg;
  lives_decoder_t *dplug=NULL;

  mainw->error=FALSE;

  if (!g_file_test (sfile->file_name, G_FILE_TEST_EXISTS)) {
    mainw->error=TRUE;
    return NULL;
  }

  // check sfile->file_name against each decoder plugin, 
  // until we get non-NULL cdata

  sfile->ext_src=NULL;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  if (!mainw->decoders_loaded) {
    mainw->decoder_list=load_decoders();
    mainw->decoders_loaded=TRUE;
  }

  decoder_plugin=mainw->decoder_list;

  dplug=(lives_decoder_t *)g_malloc(sizeof(lives_decoder_t));

  while (decoder_plugin!=NULL) {
    lives_decoder_sys_t *dpsys=(lives_decoder_sys_t *)decoder_plugin->data;

#ifdef DEBUG_DECPLUG
    g_print("trying decoder %s\n",dpsys->name);
#endif

    if ((dplug->cdata=(dpsys->get_clip_data)((tmp=(char *)g_filename_from_utf8 (sfile->file_name,-1,NULL,NULL,NULL)),
					     NULL))!=NULL) {
      g_free(tmp);
      dplug->decoder=dpsys;
      sfile->ext_src=dplug;
      if (strncmp(dpsys->name,"libzz",5)) {
	mainw->decoder_list=g_list_move_to_first(mainw->decoder_list, decoder_plugin);
      }
      break;
    }
    g_free(tmp);
    decoder_plugin=decoder_plugin->next;
  }

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

  if (sfile->ext_src!=NULL) {
    dplug=sfile->ext_src;
    msg=g_strdup_printf(" :: using decoder plugin %s",(dplug->decoder->name));
    d_print(msg);
    g_free(msg);

    return dplug->cdata;
  }

  return NULL;
}






// close one instance of dplug
void close_decoder_plugin (lives_decoder_t *dplug) {
  lives_clip_data_t *cdata;

  if (dplug==NULL) return;

  cdata=dplug->cdata;

  if (cdata!=NULL) (*dplug->decoder->clip_data_free)(cdata);

  g_free(dplug);

}


static void unload_decoder_plugin(lives_decoder_sys_t *dplug) {
  if (dplug->module_unload!=NULL) (*dplug->module_unload)();

  if (dplug->name!=NULL) {
    g_free(dplug->name);
  }

  dlclose(dplug->handle);
  g_free(dplug);
}


void unload_decoder_plugins(void) {
  GList *dplugs=mainw->decoder_list;

  while (dplugs!=NULL) {
    unload_decoder_plugin(dplugs->data);
    dplugs=dplugs->next;
  }

  g_list_free(mainw->decoder_list);
  mainw->decoder_list=NULL;
  mainw->decoders_loaded=FALSE;
}



lives_decoder_sys_t *open_decoder_plugin(const gchar *plname) {
  lives_decoder_sys_t *dplug;

  gchar *plugname;
  gboolean OK=TRUE;
  const gchar *err;

  if (!strcmp(plname,"ogg_theora_decoder")) {
    // no longer compatible
    return NULL;
  }

  dplug=(lives_decoder_sys_t *)g_malloc(sizeof(lives_decoder_sys_t));

  dplug->name=NULL;

  plugname=g_strdup_printf ("%s%s%s/%s.so",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS,plname);
  dplug->handle=dlopen(plugname,RTLD_LAZY);
  g_free (plugname);

  if (dplug->handle==NULL) {
    gchar *msg=g_strdup_printf (_("\n\nFailed to open decoder plugin %s\nError was %s\n"),plname,dlerror());
    d_print(msg);
    g_free (msg);
    g_free (dplug);
    return NULL;
  }

  if ((dplug->version=dlsym (dplug->handle,"version"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->get_clip_data=dlsym (dplug->handle,"get_clip_data"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->get_frame=dlsym (dplug->handle,"get_frame"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->clip_data_free=dlsym (dplug->handle,"clip_data_free"))==NULL) {
    OK=FALSE;
  }

  if (!OK) {
    gchar *msg=g_strdup_printf (_("\n\nDecoder plugin %s\nis missing a mandatory function.\nUnable to use it.\n"),plname);
    d_print(msg);
    g_free (msg);
    unload_decoder_plugin(dplug);
    g_free(dplug);
    return NULL;
  }

  // optional
  dplug->module_check_init=dlsym (dplug->handle,"module_check_init");
  dplug->module_unload=dlsym (dplug->handle,"module_unload");
  dplug->rip_audio=dlsym (dplug->handle,"rip_audio");
  dplug->rip_audio_cleanup=dlsym (dplug->handle,"rip_audio_cleanup");

  if (dplug->module_check_init!=NULL) {
    err=(*dplug->module_check_init)();
    
    if (err!=NULL) {
      g_snprintf(mainw->msg,512,"%s",err);
      unload_decoder_plugin(dplug);
      g_free(dplug);
      return NULL;
    }
  }

  dplug->name=g_strdup(plname);
  return dplug;
}




void get_mime_type(gchar *text, int maxlen, const lives_clip_data_t *cdata) {
  gchar *audname;
  
  if (cdata->container_name==NULL||!strlen(cdata->container_name)) g_snprintf(text,40,"%s",_("unknown"));
  else g_snprintf(text,40,"%s",cdata->container_name);

  if ((cdata->video_name==NULL||!strlen(cdata->video_name))&&(cdata->audio_name==NULL||!strlen(cdata->audio_name))) return;

  if (cdata->video_name==NULL) g_strappend(text,40,_("/unknown"));
  else {
    gchar *vidname=g_strdup_printf("/%s",cdata->video_name);
    g_strappend(text,40,vidname);
    g_free(vidname);
  }

  if (cdata->audio_name==NULL||!strlen(cdata->audio_name)) return;

  audname=g_strdup_printf("/%s",cdata->audio_name);
  g_strappend(text,40,audname);
  g_free(audname);
}




///////////////////////////////////////////////////////
// rfx plugin functions



gboolean check_rfx_for_lives (lives_rfx_t *rfx) {
  // check that an RFX is suitable for loading (cf. check_for_lives in effects-weed.c)
  if (rfx->num_in_channels==2&&rfx->props&RFX_PROPS_MAY_RESIZE) {
    gchar *tmp;
    d_print ((tmp=g_strdup_printf (_ ("Failed to load %s, transitions may not resize.\n"),rfx->name)));
    g_free(tmp);
    return FALSE;
  }
  return TRUE;
}

void do_rfx_cleanup(lives_rfx_t *rfx) {
  gchar *com;

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    com=g_strdup_printf("smogrify plugin_clear %s %d %d %s%s %s %s",cfile->handle,cfile->start,cfile->end,prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_RENDERED_EFFECTS_BUILTIN,mainw->rendered_fx->name);
    break;
  case RFX_STATUS_CUSTOM:
    com=g_strdup_printf("smogrify plugin_clear %s %d %d %s%s %s %s",cfile->handle,cfile->start,cfile->end,capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_CUSTOM,mainw->rendered_fx->name);
    break;
  default:
    com=g_strdup_printf("smogrify plugin_clear %s %d %d %s%s %s %s",cfile->handle,cfile->start,cfile->end,capable->home_dir,LIVES_CONFIG_DIR,PLUGIN_RENDERED_EFFECTS_TEST,mainw->rendered_fx->name);
    break;
  }
  dummyvar=system(com);
  g_free(com);
}




void render_fx_get_params (lives_rfx_t *rfx, const gchar *plugin_name, gshort status) {
  // create lives_param_t array from plugin supplied values
  GList *parameter_list;
  int param_idx,i;
  lives_param_t *cparam;
  gchar **param_array;
  gchar *line;
  gint len;

  switch (status) {
  case RFX_STATUS_BUILTIN:
    parameter_list=plugin_request_by_line (PLUGIN_RENDERED_EFFECTS_BUILTIN,plugin_name,"get_parameters");
    break;
  case RFX_STATUS_CUSTOM:
    parameter_list=plugin_request_by_line (PLUGIN_RENDERED_EFFECTS_CUSTOM,plugin_name,"get_parameters");
    break;
  case RFX_STATUS_SCRAP:
    parameter_list=plugin_request_by_line (PLUGIN_RFX_SCRAP,plugin_name,"get_parameters");
    break;
  default:
    parameter_list=plugin_request_by_line (PLUGIN_RENDERED_EFFECTS_TEST,plugin_name,"get_parameters");
    break;
  }

  if (parameter_list==NULL) {
    rfx->num_params=0;
    rfx->params=NULL;
    return;
  }

  pthread_mutex_lock(&mainw->gtk_mutex);
  rfx->num_params=g_list_length (parameter_list);
  rfx->params=g_malloc (rfx->num_params*sizeof(lives_param_t));
  
  for (param_idx=0;param_idx<rfx->num_params;param_idx++) {
    // TODO - error check

    line=g_list_nth_data(parameter_list,param_idx);

    len=get_token_count (line,(unsigned int)rfx->delim[0]);

    if (len<3) continue;

    param_array=g_strsplit(line,rfx->delim,-1);

    cparam=&rfx->params[param_idx];
    cparam->name=g_strdup (param_array[0]);
    cparam->label=g_strdup (param_array[1]);
    cparam->desc=NULL;
    cparam->use_mnemonic=TRUE;
    cparam->interp_func=NULL;
    cparam->display_func=NULL;
    cparam->hidden=0;
    cparam->wrap=FALSE;
    cparam->transition=FALSE;
    cparam->step_size=1.;
    cparam->copy_to=-1;
    cparam->group=0;
    cparam->max=0.;
    cparam->reinit=FALSE;
    cparam->changed=FALSE;
    cparam->change_blocked=FALSE;
    cparam->source=NULL;
    cparam->source_type=LIVES_RFX_SOURCE_RFX;

#ifdef DEBUG_RENDER_FX_P
    g_printerr("Got parameter %s\n",cparam->name);
#endif
    cparam->dp=0;
    cparam->list=NULL;
    
    if (!strncmp (param_array[2],"num",3)) {
      cparam->dp=atoi (param_array[2]+3);
      cparam->type=LIVES_PARAM_NUM;
    }
    else if (!strncmp (param_array[2],"bool",4)) {
      cparam->type=LIVES_PARAM_BOOL;
    }
    else if (!strncmp (param_array[2],"colRGB24",8)) {
      cparam->type=LIVES_PARAM_COLRGB24;
    }
    else if (!strncmp (param_array[2],"string",8)) {
      cparam->type=LIVES_PARAM_STRING;
    }
    else if (!strncmp (param_array[2],"string_list",8)) {
      cparam->type=LIVES_PARAM_STRING_LIST;
    }
    else continue;
    
    if (cparam->dp) {
      gdouble val;
      if (len<6) continue;
      val=g_strtod (param_array[3],NULL);
      cparam->value=g_malloc(sizdbl);
      cparam->def=g_malloc(sizdbl);
      set_double_param(cparam->def,val);
      set_double_param(cparam->value,val);
      cparam->min=g_strtod (param_array[4],NULL);
      cparam->max=g_strtod (param_array[5],NULL);
      if (len>6) {
	cparam->step_size=g_strtod(param_array[6],NULL);
	if (cparam->step_size==0.) cparam->step_size=1./lives_10pow(cparam->dp);
	else if (cparam->step_size<0.) {
	  cparam->step_size=-cparam->step_size;
	  cparam->wrap=TRUE;
	}
      }
    }
    else if (cparam->type==LIVES_PARAM_COLRGB24) {
      gshort red;
      gshort green;
      gshort blue;
      if (len<6) continue;
      red=(gshort)atoi (param_array[3]);
      green=(gshort)atoi (param_array[4]);
      blue=(gshort)atoi (param_array[5]);
      cparam->value=g_malloc(sizeof(lives_colRGB24_t));
      cparam->def=g_malloc(sizeof(lives_colRGB24_t));
      set_colRGB24_param(cparam->def,red,green,blue);
      set_colRGB24_param(cparam->value,red,green,blue);
    }
    else if (cparam->type==LIVES_PARAM_STRING) {
      if (len<4) continue;
      cparam->value=g_strdup(_ (param_array[3]));
      cparam->def=g_strdup(_ (param_array[3]));
      if (len>4) cparam->max=(gdouble)atoi (param_array[4]);
      if (cparam->max==0.||cparam->max>RFX_MAXSTRINGLEN) cparam->max=RFX_MAXSTRINGLEN;
    }
    else if (cparam->type==LIVES_PARAM_STRING_LIST) {
      if (len<4) continue;
      cparam->value=g_malloc(sizint);
      cparam->def=g_malloc(sizint);
      *(int *)cparam->def=atoi (param_array[3]);
      if (len>4) {
	cparam->list=array_to_string_list (param_array,3,len);
      }
      else {
	set_int_param (cparam->def,0);
      }
      set_int_param (cparam->value,get_int_param (cparam->def));
    }
    else {
      // int or bool
      gint val;
      if (len<4) continue;
      val=atoi (param_array[3]);
      cparam->value=g_malloc(sizint);
      cparam->def=g_malloc(sizint);
      set_int_param(cparam->def,val);
      set_int_param(cparam->value,val);
      if (cparam->type==LIVES_PARAM_BOOL) {
	cparam->min=0;
	cparam->max=1;
	if (len>4) cparam->group=atoi (param_array[4]);
      }
      else {
	if (len<6) continue;
	cparam->min=(gdouble)atoi (param_array[4]);
	cparam->max=(gdouble)atoi (param_array[5]);
	if (len>6) {
	  cparam->step_size=(gdouble)atoi(param_array[6]);
	  if (cparam->step_size==0.) cparam->step_size=1.;
	  else if (cparam->step_size<0.) {
	    cparam->step_size=-cparam->step_size;
	    cparam->wrap=TRUE;
	  }
	}
      }
    }
    
    for (i=0;i<MAX_PARAM_WIDGETS;i++) {
      cparam->widgets[i]=NULL;
    }
    cparam->onchange=FALSE;
    g_strfreev (param_array);
  }
  g_list_free_strings (parameter_list);
  g_list_free (parameter_list);
  pthread_mutex_unlock(&mainw->gtk_mutex);
}


GList *array_to_string_list (gchar **array, gint offset, gint len) {
  // build a GList from an array.
  int i;

  gchar *string,*tmp;
  GList *slist=NULL;

  for (i=offset+1;i<len;i++) {
    string=subst ((tmp=L2U8(array[i])),"\\n","\n");
    g_free(tmp);

    // omit a last empty string
    if (i<len-1||strlen (string)) {
      slist=g_list_append (slist, string);
    }
    else g_free(string);
  }

  return slist;
}



void sort_rfx_array (lives_rfx_t *in, gint num) {
  // sort rfx array into UTF-8 order by menu entry
  int i;
  int start=1,min_val=0;
  gboolean used[num];
  gint sorted=1;
  gchar *min_string=NULL;
  lives_rfx_t *rfx;
  gchar *tmp;

  for (i=0;i<num;i++) {
    used[i]=FALSE;
  }

  rfx=mainw->rendered_fx=(lives_rfx_t *)g_malloc ((num+1)*sizeof(lives_rfx_t));

  rfx->name=g_strdup (in[0].name);
  rfx->menu_text=g_strdup (in[0].menu_text);
  rfx->action_desc=g_strdup (in[0].action_desc);
  rfx->props=in[0].props;
  rfx->num_params=0;
  rfx->min_frames=1;
  rfx->params=NULL;
  rfx->source=NULL;
  rfx->source_type=LIVES_RFX_SOURCE_RFX;
  rfx->is_template=FALSE;
  rfx->extra=NULL;

  while (sorted<=num) {
    for (i=start;i<=num;i++) {
      if (!used[i-1]) {
	if (min_string==NULL) {
	  min_string=g_utf8_collate_key (in[i].menu_text,strlen (in[i].menu_text));
	  min_val=i;
	}
	else {
	  if (strcmp (min_string,(tmp=g_utf8_collate_key(in[i].menu_text,strlen (in[i].menu_text))))==1) {
	    g_free (min_string);
	    min_string=g_utf8_collate_key (in[i].menu_text,strlen (in[i].menu_text));
	    min_val=i;
	  }
	  g_free(tmp);
	}
      }
    }
    rfx_copy (&in[min_val],&mainw->rendered_fx[sorted++],FALSE);
    used[min_val-1]=TRUE;
    if (min_string!=NULL) g_free (min_string);
    min_string=NULL;
  }
  for (i=0;i<=num;i++) {
    rfx_free(&in[i]);
  }
}


void rfx_copy (lives_rfx_t *src, lives_rfx_t *dest, gboolean full) {
  // Warning, does not copy all fields (full will do that)
  dest->name=g_strdup (src->name);
  dest->menu_text=g_strdup (src->menu_text);
  dest->action_desc=g_strdup (src->action_desc);
  dest->min_frames=src->min_frames;
  dest->num_in_channels=src->num_in_channels;
  dest->status=src->status;
  dest->props=src->props;
  dest->source_type=src->source_type;
  dest->source=src->source;
  dest->is_template=src->is_template;
  w_memcpy(dest->delim,src->delim,2);
  if (!full) return;
}


void rfx_free(lives_rfx_t *rfx) {
  int i;

  if (rfx->name!=NULL) g_free(rfx->name);
  if (rfx->menu_text!=NULL) g_free(rfx->menu_text);
  if (rfx->action_desc!=NULL) g_free(rfx->action_desc);
  for (i=0;i<rfx->num_params;i++) {
    if (rfx->params[i].type==LIVES_PARAM_UNDISPLAYABLE) continue;
    g_free(rfx->params[i].name);
    if (rfx->params[i].def!=NULL) g_free(rfx->params[i].def);
    if (rfx->params[i].value!=NULL) g_free(rfx->params[i].value);
    if (rfx->params[i].label!=NULL) g_free(rfx->params[i].label);
    if (rfx->params[i].desc!=NULL) g_free(rfx->params[i].desc);
    if (rfx->params[i].list!=NULL) {
      g_list_free_strings(rfx->params[i].list);
      g_list_free(rfx->params[i].list);
    }
  }
  if (rfx->params!=NULL) {
    g_free(rfx->params);
  }
  if (rfx->extra!=NULL) {
    free(rfx->extra);
  }
  if (rfx->source_type==LIVES_RFX_SOURCE_WEED&&rfx->source!=NULL) {
    weed_instance_unref(rfx->source);
  }
}


void rfx_free_all (void) {
  int i;
  for (i=0;i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test;i++) {
    rfx_free(&mainw->rendered_fx[i]);
  }
  g_free(mainw->rendered_fx);
  mainw->rendered_fx=NULL;
}


void param_copy (lives_param_t *src, lives_param_t *dest, gboolean full) {
  // rfxbuilder.c uses this to copy params to a temporary copy and back again

  dest->name=g_strdup (src->name);
  dest->label=g_strdup (src->label);
  dest->group=src->group;
  dest->onchange=src->onchange;
  dest->type=src->type;
  dest->dp=src->dp;
  dest->min=src->min;
  dest->max=src->max;
  dest->step_size=src->step_size;
  dest->wrap=src->wrap;
  dest->source=src->source;
  dest->reinit=src->reinit;
  dest->source_type=src->source_type;
  dest->list=NULL;

  switch (dest->type) {
  case LIVES_PARAM_BOOL:
    dest->dp=0;
  case LIVES_PARAM_NUM:
    if (!dest->dp) {
      dest->def=g_malloc (sizint);
      w_memcpy (dest->def,src->def,sizint);
    }
    else {
      dest->def=g_malloc (sizdbl);
      w_memcpy (dest->def,src->def,sizdbl);
    }
    break;
  case LIVES_PARAM_COLRGB24:
    dest->def=g_malloc (sizeof(lives_colRGB24_t));
    w_memcpy (dest->def,src->def,sizeof(lives_colRGB24_t));
    break;
  case LIVES_PARAM_STRING:
    dest->def=g_strdup (src->def);
    break;
  case LIVES_PARAM_STRING_LIST:
    dest->def=g_malloc (sizint);
    set_int_param (dest->def,get_int_param (src->def));
    if (src->list!=NULL) dest->list=g_list_copy (src->list);
    break;
  default:
    break;
  }
  if (!full) return;
  // TODO - copy value, copy widgets

}

gboolean get_bool_param(void *value) {
  gboolean ret;
  w_memcpy(&ret,value,sizint);
  return ret;
}

gint get_int_param(void *value) {
  gint ret;
  w_memcpy(&ret,value,sizint);
  return ret;
}

gdouble get_double_param(void *value) {
  gdouble ret;
  w_memcpy(&ret,value,sizdbl);
  return ret;
}

void get_colRGB24_param(void *value, lives_colRGB24_t *rgb) {
  w_memcpy(rgb,value,sizeof(lives_colRGB24_t));
}

void get_colRGBA32_param(void *value, lives_colRGBA32_t *rgba) {
  w_memcpy(rgba,value,sizeof(lives_colRGBA32_t));
}

void set_bool_param(void *value, gboolean _const) {
  set_int_param(value,!!_const);
}

void set_int_param(void *value, gint _const) {
  w_memcpy(value,&_const,sizint);
}
void set_double_param(void *value, gdouble _const) {
  w_memcpy(value,&_const,sizdbl);

}

void set_colRGB24_param(void *value, gshort red, gshort green, gshort blue) {
  lives_colRGB24_t *rgbp=(lives_colRGB24_t *)value;

  if (red<0) red=0;
  if (red>255) red=255;
  if (green<0) green=0;
  if (green>255) green=255;
  if (blue<0) blue=0;
  if (blue>255) blue=255;

  rgbp->red=red;
  rgbp->green=green;
  rgbp->blue=blue;

}

void set_colRGBA32_param(void *value, gshort red, gshort green, gshort blue, gshort alpha) {
  lives_colRGBA32_t *rgbap=(lives_colRGBA32_t *)value;
  rgbap->red=red;
  rgbap->green=green;
  rgbap->blue=blue;
  rgbap->alpha=alpha;
}




///////////////////////////////////////////////////////////////



gint find_rfx_plugin_by_name (const gchar *name, gshort status) {
  int i;
  for (i=1;i<mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test;i++) {
    if (mainw->rendered_fx[i].name!=NULL&&!strcmp (mainw->rendered_fx[i].name,name)&&mainw->rendered_fx[i].status==status) return (gint)i;
  }
  return -1;
}



lives_param_t *weed_params_to_rfx(gint npar, weed_plant_t *plant, gboolean show_reinits) {
  int i,j;
  lives_param_t *rpar=g_malloc(npar*sizeof(lives_param_t));
  int param_hint;
  char **list;
  GList *gtk_list=NULL;
  gchar *string;
  int error;
  int vali;
  double vald;
  weed_plant_t *gui=NULL;
  int listlen;
  int cspace,*cols=NULL,red_min=0,red_max=255,green_min=0,green_max=255,blue_min=0,blue_max=255,*maxi=NULL,*mini=NULL;
  double *colsd;
  double red_mind=0.,red_maxd=1.,green_mind=0.,green_maxd=1.,blue_mind=0.,blue_maxd=1.,*maxd=NULL,*mind=NULL;
  int flags=0;
  gboolean col_int;

  weed_plant_t *wtmpl;
  weed_plant_t **wpars=NULL,*wpar=NULL;

  weed_plant_t *chann,*ctmpl;

  wpars=weed_get_plantptr_array(plant,"in_parameters",&error);

  for (i=0;i<npar;i++) {
    wpar=wpars[i];
    wtmpl=weed_get_plantptr_value(wpar,"template",&error);

    if (weed_plant_has_leaf(wtmpl,"flags")) flags=weed_get_int_value(wtmpl,"flags",&error);
    else flags=0;

    rpar[i].flags=flags;

    gui=NULL;

    if (weed_plant_has_leaf(wtmpl,"gui")) gui=weed_get_plantptr_value(wtmpl,"gui",&error);

    rpar[i].group=0;

    rpar[i].use_mnemonic=FALSE;
    rpar[i].interp_func=rpar[i].display_func=NULL;
    rpar[i].hidden=0;
    rpar[i].step_size=1.;
    rpar[i].copy_to=-1;
    rpar[i].transition=FALSE;
    rpar[i].wrap=FALSE;
    rpar[i].reinit=FALSE;
    rpar[i].change_blocked=FALSE;
    rpar[i].source=wtmpl;
    rpar[i].source_type=LIVES_RFX_SOURCE_WEED;

    if (flags&WEED_PARAMETER_VARIABLE_ELEMENTS&&!(flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL)) {
      rpar[i].hidden|=HIDDEN_MULTI;
      rpar[i].multi=PVAL_MULTI_ANY;
    }
    else if (flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL) {
      rpar[i].hidden|=HIDDEN_MULTI;
      rpar[i].multi=PVAL_MULTI_PER_CHANNEL;
    }
    else rpar[i].multi=PVAL_MULTI_NONE;

    chann=get_enabled_channel(plant,0,TRUE);
    ctmpl=weed_get_plantptr_value(chann,"template",&error);

    if (weed_plant_has_leaf(ctmpl,"is_audio")&&weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
      // dont hide multivalued params for audio effects
      rpar[i].hidden=0;
    }

    param_hint=weed_get_int_value(wtmpl,"hint",&error);

    if (gui!=NULL) {
      if (weed_plant_has_leaf(gui,"copy_value_to")) {
	int copyto=weed_get_int_value(gui,"copy_value_to",&error);
	int param_hint2,flags2=0;
	weed_plant_t *wtmpl2;
	if (copyto==i||copyto<0) copyto=-1;
	if (copyto>-1) {
	  wtmpl2=weed_get_plantptr_value(wpars[copyto],"template",&error);
	  if (weed_plant_has_leaf(wtmpl2,"flags")) flags2=weed_get_int_value(wtmpl2,"flags",&error);
	  param_hint2=weed_get_int_value(wtmpl2,"hint",&error);
	  if (param_hint==param_hint2&&((flags2&WEED_PARAMETER_VARIABLE_ELEMENTS)||(flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL&&flags2&WEED_PARAMETER_ELEMENT_PER_CHANNEL)||weed_leaf_num_elements(wtmpl,"default")==weed_leaf_num_elements(wtmpl2,"default"))) rpar[i].copy_to=copyto;
	}
      }
    }

    rpar[i].dp=0;
    rpar[i].min=0.;
    rpar[i].max=0.;
    rpar[i].list=NULL;

    if (flags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) {
      rpar[i].reinit=TRUE;
      if (!show_reinits) rpar[i].hidden|=HIDDEN_NEEDS_REINIT;
    }
    else rpar[i].reinit=FALSE;

    ///////////////////////////////

    switch (param_hint) {
    case WEED_HINT_SWITCH:
      if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
	rpar[i].hidden|=HIDDEN_MULTI;
      }
      rpar[i].type=LIVES_PARAM_BOOL;
      rpar[i].value=g_malloc(sizint);
      rpar[i].def=g_malloc(sizint);
      if (weed_plant_has_leaf(wtmpl,"host_default")) vali=weed_get_boolean_value(wtmpl,"host_default",&error); 
      else if (weed_leaf_num_elements(wtmpl,"default")>0) vali=weed_get_boolean_value(wtmpl,"default",&error);
      else vali=weed_get_boolean_value(wtmpl,"new_default",&error);
      set_int_param(rpar[i].def,vali);
      vali=weed_get_boolean_value(wpar,"value",&error);
      set_int_param(rpar[i].value,vali);
      if (weed_plant_has_leaf(wtmpl,"group")) rpar[i].group=weed_get_int_value(wtmpl,"group",&error);
      break;
    case WEED_HINT_INTEGER:
      if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
	rpar[i].hidden|=HIDDEN_MULTI;
      }
      rpar[i].type=LIVES_PARAM_NUM;
      rpar[i].value=g_malloc(sizint);
      rpar[i].def=g_malloc(sizint);
      if (weed_plant_has_leaf(wtmpl,"host_default")) {
	vali=weed_get_int_value(wtmpl,"host_default",&error);
      }
      else if (weed_leaf_num_elements(wtmpl,"default")>0) vali=weed_get_int_value(wtmpl,"default",&error);
      else vali=weed_get_int_value(wtmpl,"new_default",&error);
      set_int_param(rpar[i].def,vali);
      vali=weed_get_int_value(wpar,"value",&error);
      set_int_param(rpar[i].value,vali);
      rpar[i].min=(gdouble)weed_get_int_value(wtmpl,"min",&error);
      rpar[i].max=(gdouble)weed_get_int_value(wtmpl,"max",&error);
      if (weed_plant_has_leaf(wtmpl,"wrap")&&weed_get_boolean_value(wtmpl,"wrap",&error)==WEED_TRUE) rpar[i].wrap=TRUE;
      if (gui!=NULL) {
	if (weed_plant_has_leaf(gui,"choices")) {
	  listlen=weed_leaf_num_elements(gui,"choices");
	  list=weed_get_string_array(gui,"choices",&error);
	  for (j=0;j<listlen;j++) {
	    gtk_list=g_list_append(gtk_list,g_strdup(list[j]));
	    weed_free(list[j]);
	  }
	  weed_free(list);
	  rpar[i].list=g_list_copy(gtk_list);
	  g_list_free(gtk_list);
	  gtk_list=NULL;
	  rpar[i].type=LIVES_PARAM_STRING_LIST;
	}
	else if (weed_plant_has_leaf(gui,"step_size")) rpar[i].step_size=(gdouble)weed_get_int_value(gui,"step_size",&error);
	if (rpar[i].step_size==0.) rpar[i].step_size=1.;
      }
      break;
    case WEED_HINT_FLOAT:
      if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
	rpar[i].hidden|=HIDDEN_MULTI;
      }
      rpar[i].type=LIVES_PARAM_NUM;
      rpar[i].value=g_malloc(sizdbl);
      rpar[i].def=g_malloc(sizdbl);
      if (weed_plant_has_leaf(wtmpl,"host_default")) vald=weed_get_double_value(wtmpl,"host_default",&error); 
      else if (weed_leaf_num_elements(wtmpl,"default")>0) vald=weed_get_double_value(wtmpl,"default",&error);
      else vald=weed_get_double_value(wtmpl,"new_default",&error);
      set_double_param(rpar[i].def,vald);
      vald=weed_get_double_value(wpar,"value",&error);
      set_double_param(rpar[i].value,vald);
      rpar[i].min=weed_get_double_value(wtmpl,"min",&error);
      rpar[i].max=weed_get_double_value(wtmpl,"max",&error);
      if (weed_plant_has_leaf(wtmpl,"wrap")&&weed_get_boolean_value(wtmpl,"wrap",&error)==WEED_TRUE) rpar[i].wrap=TRUE;
      rpar[i].step_size=0.;
      if (gui!=NULL) {
	if (weed_plant_has_leaf(gui,"step_size")) rpar[i].step_size=weed_get_double_value(gui,"step_size",&error);
	if (weed_plant_has_leaf(gui,"decimals")) rpar[i].dp=weed_get_int_value(gui,"decimals",&error);
      }
      if (rpar[i].dp==0) rpar[i].dp=2;
      if (rpar[i].step_size==0.) rpar[i].step_size=1./lives_10pow(rpar[i].dp);
      break;
    case WEED_HINT_TEXT:
      if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
	rpar[i].hidden|=HIDDEN_MULTI;
      }
      rpar[i].type=LIVES_PARAM_STRING;
      if (weed_plant_has_leaf(wtmpl,"host_default")) string=weed_get_string_value(wtmpl,"host_default",&error); 
      else if (weed_leaf_num_elements(wtmpl,"default")>0) string=weed_get_string_value(wtmpl,"default",&error);
      else string=weed_get_string_value(wtmpl,"new_default",&error);
      rpar[i].def=g_strdup(string);
      weed_free(string);
      string=weed_get_string_value(wpar,"value",&error);
      rpar[i].value=g_strdup(string);

      weed_free(string);
      rpar[i].max=0.;
      if (gui!=NULL&&weed_plant_has_leaf(gui,"maxchars")) {
	rpar[i].max=(gdouble)weed_get_int_value(gui,"maxchars",&error);
	if (rpar[i].max<0.) rpar[i].max=0.;
      }
      break;
    case WEED_HINT_COLOR:
      cspace=weed_get_int_value(wtmpl,"colorspace",&error);
      switch (cspace) {
      case WEED_COLORSPACE_RGB:
	if (weed_leaf_num_elements(wtmpl,"default")>3) {
	  rpar[i].hidden|=HIDDEN_MULTI;
	}
	rpar[i].type=LIVES_PARAM_COLRGB24;
	rpar[i].value=g_malloc(3*sizint);
	rpar[i].def=g_malloc(3*sizint);

	if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
	  if (weed_plant_has_leaf(wtmpl,"host_default")) cols=weed_get_int_array(wtmpl,"host_default",&error); 
	  else if (weed_leaf_num_elements(wtmpl,"default")>0) cols=weed_get_int_array(wtmpl,"default",&error);
	  else cols=weed_get_int_array(wtmpl,"new_default",&error);
	  if (weed_leaf_num_elements(wtmpl,"max")==1) {
	    red_max=green_max=blue_max=weed_get_int_value(wtmpl,"max",&error);
	  }
	  else {
	    maxi=weed_get_int_array(wtmpl,"max",&error);
	    red_max=maxi[0];
	    green_max=maxi[1];
	    blue_max=maxi[2];
	  }
	  if (weed_leaf_num_elements(wtmpl,"min")==1) {
	    red_min=green_min=blue_min=weed_get_int_value(wtmpl,"min",&error);
	  }
	  else {
	    mini=weed_get_int_array(wtmpl,"min",&error);
	    red_min=mini[0];
	    green_min=mini[1];
	    blue_min=mini[2];
	  }
	  if (cols[0]<red_min) cols[0]=red_min;
	  if (cols[1]<green_min) cols[1]=green_min;
	  if (cols[2]<blue_min) cols[2]=blue_min;
	  if (cols[0]>red_max) cols[0]=red_max;
	  if (cols[1]>green_max) cols[1]=green_max;
	  if (cols[2]>blue_max) cols[2]=blue_max;
	  cols[0]=(cols[0]-red_min)/(red_max-red_min)*255;
	  cols[1]=(cols[1]-green_min)/(green_max-green_min)*255;
	  cols[2]=(cols[2]-blue_min)/(blue_max-blue_min)*255;
	  col_int=TRUE;
	}
	else {
	  if (weed_plant_has_leaf(wtmpl,"host_default")) colsd=weed_get_double_array(wtmpl,"host_default",&error); 
	  else if (weed_leaf_num_elements(wtmpl,"default")>0) colsd=weed_get_double_array(wtmpl,"default",&error);
	  else colsd=weed_get_double_array(wtmpl,"default",&error);
	  if (weed_leaf_num_elements(wtmpl,"max")==1) {
	    red_maxd=green_maxd=blue_maxd=weed_get_double_value(wtmpl,"max",&error);
	  }
	  else {
	    maxd=weed_get_double_array(wtmpl,"max",&error);
	    red_maxd=maxd[0];
	    green_maxd=maxd[1];
	    blue_maxd=maxd[2];
	  }
	  if (weed_leaf_num_elements(wtmpl,"min")==1) {
	    red_mind=green_mind=blue_mind=weed_get_double_value(wtmpl,"min",&error);
	  }
	  else {
	    mind=weed_get_double_array(wtmpl,"min",&error);
	    red_mind=mind[0];
	    green_mind=mind[1];
	    blue_mind=mind[2];
	  }
	  if (colsd[0]<red_mind) colsd[0]=red_mind;
	  if (colsd[1]<green_mind) colsd[1]=green_mind;
	  if (colsd[2]<blue_mind) colsd[2]=blue_mind;
	  if (colsd[0]>red_maxd) colsd[0]=red_maxd;
	  if (colsd[1]>green_maxd) colsd[1]=green_maxd;
	  if (colsd[2]>blue_maxd) colsd[2]=blue_maxd;
	  cols=weed_malloc(3*sizshrt);
	  cols[0]=(colsd[0]-red_mind)/(red_maxd-red_mind)*255.+.5;
	  cols[1]=(colsd[1]-green_mind)/(green_maxd-green_mind)*255.+.5;
	  cols[2]=(colsd[2]-blue_mind)/(blue_maxd-blue_mind)*255.+.5;
	  col_int=FALSE;
	}
	set_colRGB24_param(rpar[i].def,cols[0],cols[1],cols[2]);
	if (col_int) {
	  weed_free(cols);
	  cols=weed_get_int_array(wpar,"value",&error);
	  if (cols[0]<red_min) cols[0]=red_min;
	  if (cols[1]<green_min) cols[1]=green_min;
	  if (cols[2]<blue_min) cols[2]=blue_min;
	  if (cols[0]>red_max) cols[0]=red_max;
	  if (cols[1]>green_max) cols[1]=green_max;
	  if (cols[2]>blue_max) cols[2]=blue_max;
	  cols[0]=(cols[0]-red_min)/(red_max-red_min)*255;
	  cols[1]=(cols[1]-green_min)/(green_max-green_min)*255;
	  cols[2]=(cols[2]-blue_min)/(blue_max-blue_min)*255;
	}
	else {
	  colsd=weed_get_double_array(wpar,"value",&error);
	  if (colsd[0]<red_mind) colsd[0]=red_mind;
	  if (colsd[1]<green_mind) colsd[1]=green_mind;
	  if (colsd[2]<blue_mind) colsd[2]=blue_mind;
	  if (colsd[0]>red_maxd) colsd[0]=red_maxd;
	  if (colsd[1]>green_maxd) colsd[1]=green_maxd;
	  if (colsd[2]>blue_maxd) colsd[2]=blue_maxd;
	  cols[0]=(colsd[0]-red_mind)/(red_maxd-red_mind)*255.+.5;
	  cols[1]=(colsd[1]-green_mind)/(green_maxd-green_mind)*255.+.5;
	  cols[2]=(colsd[2]-blue_mind)/(blue_maxd-blue_mind)*255.+.5;
	}
	set_colRGB24_param(rpar[i].value,(gshort)cols[0],(gshort)cols[1],(gshort)cols[2]);
	weed_free(cols);
	if (maxi!=NULL) weed_free(maxi);
	if (mini!=NULL) weed_free(mini);
	if (maxd!=NULL) weed_free(maxd);
	if (mind!=NULL) weed_free(mind);
	break;
      }
      break;

    default:
      rpar[i].type=LIVES_PARAM_UNKNOWN; // TODO - try to get default
    }

    string=weed_get_string_value(wtmpl,"name",&error);
    rpar[i].name=g_strdup(string);
    rpar[i].label=g_strdup(string);
    weed_free(string);

    if (weed_plant_has_leaf(wtmpl,"description")) {
      string=weed_get_string_value(wtmpl,"description",&error);
      rpar[i].desc=g_strdup(string);
      weed_free(string);
    }
    else rpar[i].desc=NULL;

    // gui part /////////////////////

    if (gui!=NULL) {
      if (weed_plant_has_leaf(gui,"label")) {
	string=weed_get_string_value(gui,"label",&error);
	g_free(rpar[i].label);
	rpar[i].label=g_strdup(string);
	weed_free(string);
      }
      if (weed_plant_has_leaf(gui,"use_mnemonic")) rpar[i].use_mnemonic=weed_get_boolean_value(gui,"use_mnemonic",&error);
      if (weed_plant_has_leaf(gui,"hidden")) rpar[i].hidden|=((weed_get_boolean_value(gui,"hidden",&error)==WEED_TRUE)*HIDDEN_GUI);
      if (weed_plant_has_leaf(gui,"display_func")) {
	weed_display_f *display_func_ptr_ptr;
	weed_display_f display_func;
	weed_leaf_get(gui,"display_func",0,(void *)&display_func_ptr_ptr);
	display_func=display_func_ptr_ptr[0];
	rpar[i].display_func=display_func;
      }
      if (weed_plant_has_leaf(gui,"interpolate_func")) {
	weed_interpolate_f *interp_func_ptr_ptr;
	weed_interpolate_f interp_func;
	weed_leaf_get(gui,"interpolate_func",0,(void *)&interp_func_ptr_ptr);
	interp_func=interp_func_ptr_ptr[0];
	rpar[i].interp_func=interp_func;
      }
    }

    for (j=0;j<MAX_PARAM_WIDGETS;j++) {
      rpar[i].widgets[j]=NULL;
    }
    rpar[i].onchange=FALSE;
  }

  for (i=0;i<npar;i++) {
    if (rpar[i].copy_to!=-1) weed_leaf_copy(weed_inst_in_param(plant,rpar[i].copy_to,FALSE),"value",weed_inst_in_param(plant,i,FALSE),"value");
  }

  weed_free(wpars);

  return rpar;
}



lives_rfx_t *weed_to_rfx (weed_plant_t *plant, gboolean show_reinits) {
  // return an RFX for a weed effect; set rfx->source to an INSTANCE of the filter
  int error;
  weed_plant_t *filter;

  gchar *string;
  lives_rfx_t *rfx=g_malloc(sizeof(lives_rfx_t));
  rfx->is_template=FALSE;
  if (weed_get_int_value(plant,"type",&error)==WEED_PLANT_FILTER_INSTANCE) 
    filter=weed_get_plantptr_value(plant,"filter_class",&error);
  else {
    filter=plant;
    plant=weed_instance_from_filter(filter);
    // init and deinit the effect to allow the plugin to hide parameters, etc.
    weed_reinit_effect(plant,FALSE);
    rfx->is_template=TRUE;
  }

  string=weed_get_string_value(filter,"name",&error);
  rfx->name=g_strdup(string);
  rfx->menu_text=g_strdup(string);
  weed_free(string);
  rfx->action_desc=g_strdup("no action");
  rfx->min_frames=-1;
  rfx->num_in_channels=enabled_in_channels(plant,FALSE);
  rfx->status=RFX_STATUS_WEED;
  rfx->props=0;
  rfx->menuitem=NULL;
  if (!weed_plant_has_leaf(filter,"in_parameter_templates")||weed_get_plantptr_value(filter,"in_parameter_templates",&error)==NULL) rfx->num_params=0;
  else rfx->num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
  if (rfx->num_params>0) rfx->params=weed_params_to_rfx(rfx->num_params,plant,show_reinits);
  else rfx->params=NULL;
  rfx->source=(void *)plant;
  rfx->source_type=LIVES_RFX_SOURCE_WEED;
  rfx->extra=NULL;
  return rfx;
}



GList *get_external_window_hints(lives_rfx_t *rfx) {
  GList *hints=NULL;

  if (rfx->status==RFX_STATUS_WEED) {
    int i,error;
    int num_hints;
    weed_plant_t *gui;
    weed_plant_t *inst=rfx->source;
    weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
    char *string,**rfx_strings,*delim;
   
    if (!weed_plant_has_leaf(filter,"gui")) return NULL;
    gui=weed_get_plantptr_value(filter,"gui",&error);

    if (!weed_plant_has_leaf(gui,"layout_scheme")) return NULL;

    string=weed_get_string_value(gui,"layout_scheme",&error);
    if (strcmp(string,"RFX")) {
      weed_free(string);
      return NULL;
    }
    weed_free(string);

    if (!weed_plant_has_leaf(gui,"rfx_delim")) return NULL;
    delim=weed_get_string_value(gui,"rfx_delim",&error);
    g_snprintf(rfx->delim,2,"%s",delim);
    weed_free(delim);

    if (!weed_plant_has_leaf(gui,"rfx_strings")) return NULL;

    num_hints=weed_leaf_num_elements(gui,"rfx_strings");

    if (num_hints==0) return NULL;
    rfx_strings=weed_get_string_array(gui,"rfx_strings",&error);

    for (i=0;i<num_hints;i++) {
      hints=g_list_append(hints,g_strdup(rfx_strings[i]));
      weed_free(rfx_strings[i]);
    }
    weed_free(rfx_strings);
  }
  return hints;
}






gchar *plugin_run_param_window(const gchar *get_com, GtkVBox *vbox, lives_rfx_t **ret_rfx) {

  // here we create an rfx script from some fixed values and values from the plugin; we will then compile the script to an rfx scrap and use the scrap to get info
  // about additional parameters, and create the parameter window
  
  // this is done like so to allow use of plugins written in any language; they need only output an RFX scriptlet on stdout when called from the commandline


  // the param window is run, and the marshalled values are returned

  // if the user closes the window with Cancel, NULL is returned instead


  FILE *sfile;
  lives_rfx_t *rfx=(lives_rfx_t *)g_malloc(sizeof(lives_rfx_t));
  gchar *string;
  gchar *rfx_scrapname=g_strdup_printf("rfx.%d",getpid());
  gchar *rfxfile=g_strdup_printf ("%s/.%s.script",prefs->tmpdir,rfx_scrapname);
  int res;
  gchar *com;
  gchar *res_string=NULL;
  gchar buff[32];

  string=g_strdup_printf("<name>\n%s\n</name>\n",rfx_scrapname);
  sfile=fopen(rfxfile,"w");
  fputs(string,sfile);
  fclose(sfile);
  g_free(string);

  com=g_strdup_printf("%s >>%s",get_com,rfxfile);
  dummyvar=system(com);
  g_free(com);

  // OK, we should now have an RFX fragment in a file, we can compile it, then build a parameter window from it
    
  com=g_strdup_printf("%s %s %s >/dev/null",RFX_BUILDER,rfxfile,prefs->tmpdir);
  res=system(com);
  g_free(com);
    
  unlink(rfxfile);
  g_free(rfxfile);

  if (res==0) {
    // the script compiled correctly

    // now we pop up the parameter window, get the values of our parameters, and marshall them as extra_params
      
    // first create a lives_rfx_t from the scrap
    rfx->name=g_strdup(rfx_scrapname);
    rfx->action_desc=rfx->extra=NULL;
    rfx->status=RFX_STATUS_SCRAP;

    rfx->num_in_channels=0;
    rfx->min_frames=-1;

    // get the delimiter
    rfxfile=g_strdup_printf("%ssmdef.%d",prefs->tmpdir,getpid());
    com=g_strdup_printf("%s%s get_define > %s",prefs->tmpdir,rfx_scrapname,rfxfile);
    dummyvar=system(com);
    g_free(com);

    sfile=fopen(rfxfile,"r");
    dummychar=fgets(buff,32,sfile);
    fclose(sfile);

    unlink(rfxfile);
    g_free(rfxfile);

    g_snprintf(rfx->delim,2,"%s",buff);

    rfx->menu_text=(vbox==NULL?g_strdup_printf(_("%s advanced settings"),prefs->encoder.of_desc):g_strdup(""));
    rfx->is_template=FALSE;

    rfx->source=NULL;
    rfx->source_type=LIVES_RFX_SOURCE_RFX;
      
    render_fx_get_params(rfx,rfx_scrapname,RFX_STATUS_SCRAP);

    // now we build our window and get param values
    if (vbox==NULL) {
      on_render_fx_pre_activate(NULL,rfx);

      if (prefs->show_gui) {
	if (mainw->multitrack==NULL) gtk_window_set_transient_for(GTK_WINDOW(fx_dialog[1]),GTK_WINDOW(mainw->LiVES));
	else gtk_window_set_transient_for(GTK_WINDOW(fx_dialog[1]),GTK_WINDOW(mainw->multitrack->window));
      }
      gtk_window_set_modal (GTK_WINDOW (fx_dialog[1]), TRUE);
      
      if (gtk_dialog_run(GTK_DIALOG(fx_dialog[1]))==GTK_RESPONSE_OK) {
	// marshall our params for passing to the plugin
	res_string=param_marshall(rfx,FALSE);
      }
    }
    else {
      make_param_box(vbox,rfx);
    }
      
    rfxfile=g_strdup_printf ("%s/%s",prefs->tmpdir,rfx_scrapname);
    unlink(rfxfile);
    g_free(rfxfile);

    if (ret_rfx!=NULL) {
      *ret_rfx=rfx;
    }
    else {
      rfx_free(rfx);
      g_free(rfx);
    }
  }
  else {
    if (ret_rfx!=NULL) {
      *ret_rfx=NULL;
    }
    else {
      res_string=g_strdup("");
    }
    if (rfx!=NULL) {
      g_free(rfx);
    }
  }

  g_free(rfx_scrapname);
  return res_string;
}
