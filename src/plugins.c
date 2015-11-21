// plugins.c
// LiVES
// (c) G. Finch 2003 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <dlfcn.h>
#include <errno.h>


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#include <weed/weed-host.h>
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
#include "effects.h"
#include "interface.h"

#include "rfx-builder.h"
#include "paramwindow.h"

const char *anames[AUDIO_CODEC_MAX]= {"mp3","pcm","mp2","vorbis","AC3","AAC","AMR_NB","raw","wma2",""};

static boolean list_plugins;


///////////////////////
// command-line plugins


static LiVESList *get_plugin_result(const char *command, const char *delim, boolean allow_blanks) {

  LiVESList *list=NULL;
  char **array;
  ssize_t bytes=0;
  int pieces;
  int outfile_fd,i;
  int retval;
  int alarm_handle;
  int error;
  boolean timeout;

  char *msg,*buf;

  char *outfile;
  char *com;
  char buffer[65536];

  threaded_dialog_spin();

#ifndef IS_MINGW
  outfile=lives_strdup_printf("%s/.smogplugin.%d",prefs->tmpdir,capable->mainpid);
#else
  outfile=lives_strdup_printf("%s/smogplugin.%d",prefs->tmpdir,capable->mainpid);
#endif

  unlink(outfile);

  com=lives_strconcat(command," > \"",outfile,"\"",NULL);

  mainw->error=FALSE;

  if ((error=system(com))!=0&&error!=126*256&&error!=256) {
    if (!list_plugins) {
      char *msg2;
      lives_free(com);
      if (mainw->is_ready) {
        if ((outfile_fd=open(outfile,O_RDONLY))>-1) {
          bytes=read(outfile_fd,&buffer,65535);
          if (bytes<0) bytes=0;
          close(outfile_fd);
          unlink(outfile);
          memset(buffer+bytes,0,1);
        }
        msg=lives_strdup_printf(_("\nPlugin error: %s failed with code %d"),command,error/256);
        if (bytes) {
          msg2=lives_strconcat(msg,lives_strdup_printf(_(" : message was %s\n"),buffer),NULL);
          lives_snprintf(mainw->msg,512,"%s",buffer);
        } else {
          msg2=lives_strconcat(msg,"\n",NULL);
        }
        d_print(msg2);
        lives_free(msg2);
        lives_free(msg);
      }
    }
    lives_free(outfile);
    threaded_dialog_spin();
    unlink(outfile);
    return list;
  }
  lives_free(com);
  if (!lives_file_test(outfile, LIVES_FILE_TEST_EXISTS)) {
    lives_free(outfile);
    threaded_dialog_spin();
    return NULL;
  }
  threaded_dialog_spin();

  do {
    retval=0;
    timeout=FALSE;

#define LIVES_PLUGIN_TIMEOUT  (20 * U_SEC) // 20 sec

    alarm_handle=lives_alarm_set(LIVES_PLUGIN_TIMEOUT);

    while ((outfile_fd=open(outfile,O_RDONLY))==-1&&!(timeout=lives_alarm_get(alarm_handle))) {
      lives_usleep(prefs->sleep_time);
    }

    lives_alarm_clear(alarm_handle);

    if (timeout) {
      msg=lives_strdup_printf(("Plugin timed out on message %s"),command);
      LIVES_ERROR(msg);
      lives_free(msg);
      retval=do_read_failed_error_s_with_retry(outfile,NULL,NULL);
    } else {
      bytes=read(outfile_fd,&buffer,65535);
      close(outfile_fd);
      unlink(outfile);

      if (bytes<0) {
        retval=do_read_failed_error_s_with_retry(outfile,NULL,NULL);
      } else {
        threaded_dialog_spin();
        memset(buffer+bytes,0,1);
      }
    }
  } while (retval==LIVES_RESPONSE_RETRY);

  unlink(outfile);
  lives_free(outfile);

  if (retval==LIVES_RESPONSE_CANCEL) {
    threaded_dialog_spin();
    return list;
  }

#ifdef DEBUG_PLUGINS
  lives_printerr("plugin msg: %s %d\n",buffer,error);
#endif

  if (error==256) {
    mainw->error=TRUE;
    lives_snprintf(mainw->msg,512,"%s",buffer);
    return list;
  }

  pieces=get_token_count(buffer,delim[0]);
  array=lives_strsplit(buffer,delim,pieces);
  for (i=0; i<pieces; i++) {
    if (array[i]!=NULL) {
      buf=lives_strdup(lives_strstrip(array[i]));
      if (strlen(buf)||allow_blanks) {
        list=lives_list_append(list, buf);
      } else lives_free(buf);
    }
  }
  lives_strfreev(array);
  threaded_dialog_spin();
  return list;
}


LiVESList *plugin_request_with_blanks(const char *plugin_type, const char *plugin_name, const char *request) {
  // allow blanks in a list
  return plugin_request_common(plugin_type, plugin_name, request, "|", TRUE);
}

LiVESList *plugin_request(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "|", FALSE);
}

LiVESList *plugin_request_by_line(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, "\n", FALSE);
}

LiVESList *plugin_request_by_space(const char *plugin_type, const char *plugin_name, const char *request) {
  return plugin_request_common(plugin_type, plugin_name, request, " ", FALSE);
}



LiVESList *plugin_request_common(const char *plugin_type, const char *plugin_name, const char *request,
                                 const char *delim, boolean allow_blanks) {
  // returns a LiVESList of responses to -request, or NULL on error
  // by_line says whether we split on '\n' or on '|'

  // NOTE: request must not be quoted here, since it contains a list of parameters
  // instead, caller should ensure that any strings in *request are suitably escaped and quoted
  // e.g. by calling param_marshall()

  LiVESList *reslist=NULL;
  char *com,*comfile;

#ifdef IS_MINGW
  char *ext,*cmd;
#endif

  if (plugin_type!=NULL) {

    if (plugin_name==NULL||!strlen(plugin_name)) {
      return reslist;
    }

    // some types live in home directory...
    if (!strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM)||!strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_TEST)) {
      comfile=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,plugin_type,plugin_name,NULL);
    } else if (!strcmp(plugin_type,PLUGIN_RFX_SCRAP)) {
      // scraps are in the tmpdir
      comfile=lives_build_filename(prefs->tmpdir,plugin_name,NULL);
    } else {
      comfile=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,plugin_type,plugin_name,NULL);
    }

#ifndef IS_MINGW
    //#define DEBUG_PLUGINS
#ifdef DEBUG_PLUGINS
    com=lives_strdup_printf("\"%s\" %s",comfile,request);
    lives_printerr("will run: %s\n",com);
#else
    com=lives_strdup_printf("\"%s\" %s 2>/dev/null",comfile,request);
#endif

#else
    // check by file extension

    ext=get_extension(comfile);
    if (!strcmp(ext,"py")) {
      if (!capable->has_python) {
        lives_free(ext);
        lives_free(comfile);
        return reslist;
      }
      cmd=lives_strdup("python");
    }

    else cmd=lives_strdup("perl");

    //#define DEBUG_PLUGINS
#ifdef DEBUG_PLUGINS
    com=lives_strdup_printf("%s \"%s\" %s",cmd,comfile,request);
    lives_printerr("will run: %s\n",com);
#else
    com=lives_strdup_printf("%s \"%s\" %s 2>NUL",cmd,comfile,request);
#endif

    lives_free(ext);
    lives_free(cmd);

#endif

    lives_free(comfile);
  } else com=lives_strdup(request);
  list_plugins=FALSE;
  reslist=get_plugin_result(com,delim,allow_blanks);
  lives_free(com);
  threaded_dialog_spin();
  return reslist;
}


//////////////////
// get list of plugins of various types

LiVESList *get_plugin_list(const char *plugin_type, boolean allow_nonex, const char *plugdir, const char *filter_ext) {
  // returns a LiVESList * of plugins of type plugin_type
  // returns empty list if there are no plugins of that type

  // allow_nonex to allow non-executable files (e.g. libs)
  // filter_ext can be non-NULL to filter for files ending .filter_ext

  // TODO - use enum for plugin_type

  char *com,*tmp;
  LiVESList *pluglist;

  const char *ext=(filter_ext==NULL)?"":filter_ext;

  if (!strcmp(plugin_type,PLUGIN_THEMES)) {
    com=lives_strdup_printf("%s list_plugins 0 1 \"%s%s\" \"\"",prefs->backend_sync,prefs->prefix_dir,THEME_DIR);
  } else if (!strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM_SCRIPTS)||
             !strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_TEST_SCRIPTS)||
             !strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_CUSTOM)||
             !strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_TEST)||
             !strcmp(plugin_type,PLUGIN_COMPOUND_EFFECTS_CUSTOM)
            ) {
    // look in home
    com=lives_strdup_printf("%s list_plugins %d 0 \"%s/%s%s\" \"%s\"",prefs->backend_sync,allow_nonex,capable->home_dir,
                            LIVES_CONFIG_DIR,plugin_type,ext);
  } else if (!strcmp(plugin_type,PLUGIN_EFFECTS_WEED)) {
    com=lives_strdup_printf("%s list_plugins 1 1 \"%s\" \"%s\"",prefs->backend_sync,
                            (tmp=lives_filename_from_utf8((char *)plugdir,-1,NULL,NULL,NULL)),ext);
    lives_free(tmp);
  } else if (!strcmp(plugin_type,PLUGIN_DECODERS)) {
    com=lives_strdup_printf("%s list_plugins 1 0 \"%s\" \"%s\"",prefs->backend_sync,
                            (tmp=lives_filename_from_utf8((char *)plugdir,-1,NULL,NULL,NULL)),ext);
    lives_free(tmp);
  } else if (!strcmp(plugin_type,PLUGIN_RENDERED_EFFECTS_BUILTIN_SCRIPTS)) {
    com=lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"",prefs->backend_sync,allow_nonex,prefs->prefix_dir,
                            PLUGIN_SCRIPTS_DIR,plugin_type,ext);
  } else if (!strcmp(plugin_type,PLUGIN_COMPOUND_EFFECTS_BUILTIN)) {
    com=lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"",prefs->backend_sync,allow_nonex,prefs->prefix_dir,
                            PLUGIN_COMPOUND_DIR,plugin_type,ext);
  } else {
    com=lives_strdup_printf("%s list_plugins %d 0 \"%s%s%s\" \"%s\"",prefs->backend_sync,allow_nonex,prefs->lib_dir,
                            PLUGIN_EXEC_DIR,plugin_type,ext);
  }
  list_plugins=TRUE;

  //g_print("\n\n\nLIST CMD: %s\n",com);

  pluglist=get_plugin_result(com,"|",FALSE);
  lives_free(com);
  threaded_dialog_spin();
  return pluglist;
}



///////////////////
// video plugins


void save_vpp_defaults(_vid_playback_plugin *vpp, char *vpp_file) {
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
  int32_t len;
  const char *version;
  int i;
  char *msg;
  int intzero=0;
  double dblzero=0.;

  if (mainw->vpp==NULL) {
    unlink(vpp_file);
    return;
  }

  if ((fd=open(vpp_file,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))==-1) {
    msg=lives_strdup_printf(_("\n\nUnable to write video playback plugin defaults file\n%s\nError code %d\n"),vpp_file,errno);
    LIVES_ERROR(msg);
    lives_free(msg);
    return;
  }

#ifdef IS_MINGW
  setmode(fd, O_BINARY);
#endif

  msg=lives_strdup_printf(_("Updating video playback plugin defaults in %s\n"),vpp_file);
  LIVES_INFO(msg);
  lives_free(msg);

  msg=lives_strdup("LiVES vpp defaults file version 2\n");
  if (!lives_write(fd,msg,strlen(msg),FALSE)) return;
  lives_free(msg);

  len=strlen(mainw->vpp->name);
  if (lives_write_le(fd,&len,4,FALSE)<4) return;
  if (lives_write(fd,mainw->vpp->name,len,FALSE)<len) return;

  version=(*mainw->vpp->version)();
  len=strlen(version);
  if (lives_write_le(fd,&len,4,FALSE)<4) return;
  if (lives_write(fd,version,len,FALSE)<len) return;

  if (lives_write_le(fd,&(mainw->vpp->palette),4,FALSE)<4) return;
  if (lives_write_le(fd,&(mainw->vpp->YUV_sampling),4,FALSE)<4) return;
  if (lives_write_le(fd,&(mainw->vpp->YUV_clamping),4,FALSE)<4) return;
  if (lives_write_le(fd,&(mainw->vpp->YUV_subspace),4,FALSE)<4) return;

  if (lives_write_le(fd,mainw->vpp->fwidth<=0?&intzero:&(mainw->vpp->fwidth),4,FALSE)<4) return;
  if (lives_write_le(fd,mainw->vpp->fheight<=0?&intzero:&(mainw->vpp->fheight),4,FALSE)<4) return;

  if (lives_write_le(fd,mainw->vpp->fixed_fpsd<=0.?&dblzero:&(mainw->vpp->fixed_fpsd),8,FALSE)<8) return;
  if (lives_write_le(fd,mainw->vpp->fixed_fps_numer<=0?&intzero:&(mainw->vpp->fixed_fps_numer),4,FALSE)<4) return;
  if (lives_write_le(fd,mainw->vpp->fixed_fps_denom<=0?&intzero:&(mainw->vpp->fixed_fps_denom),4,FALSE)<4) return;

  if (lives_write_le(fd,&(mainw->vpp->extra_argc),4,FALSE)<4) return;

  for (i=0; i<mainw->vpp->extra_argc; i++) {
    len=strlen(mainw->vpp->extra_argv[i]);
    if (lives_write_le(fd,&len,4,FALSE)<4) return;
    if (lives_write(fd,mainw->vpp->extra_argv[i],len,FALSE)<len) return;
  }

  close(fd);

}


void load_vpp_defaults(_vid_playback_plugin *vpp, char *vpp_file) {
  ssize_t len;
  const char *version;

  char buf[512];

  char *msg;

  int retval;
  int fd;

  register int i;

  if (!lives_file_test(vpp_file,LIVES_FILE_TEST_EXISTS)) {
    return;
  }

  d_print(_("Loading video playback plugin defaults from %s..."),vpp_file);

  do {
    retval=0;
    if ((fd=open(vpp_file,O_RDONLY))==-1) {
      retval=do_read_failed_error_s_with_retry(vpp_file,lives_strerror(errno),NULL);
      if (retval==LIVES_RESPONSE_CANCEL) {
        mainw->vpp=NULL;
        return;
      }
    } else {
      do {

#ifdef IS_MINGW
        setmode(fd, O_BINARY);
#endif

        mainw->read_failed=FALSE;
        msg=lives_strdup("LiVES vpp defaults file version 2\n");
        len=lives_read(fd,buf,strlen(msg),FALSE);
        if (len<0) len=0;
        memset(buf+len,0,1);

        if (mainw->read_failed) break;

        // identifier string
        if (strcmp(msg,buf)) {
          lives_free(msg);
          d_print_file_error_failed();
          close(fd);
          return;
        }
        lives_free(msg);

        // plugin name
        lives_read_le(fd,&len,4,FALSE);
        if (mainw->read_failed) break;
        lives_read(fd,buf,len,FALSE);
        memset(buf+len,0,1);

        if (mainw->read_failed) break;

        if (strcmp(buf,mainw->vpp->name)) {
          d_print_file_error_failed();
          close(fd);
          return;
        }


        // version string
        version=(*mainw->vpp->version)();
        lives_read_le(fd,&len,4,FALSE);
        if (mainw->read_failed) break;
        lives_read(fd,buf,len,FALSE);

        if (mainw->read_failed) break;

        memset(buf+len,0,1);

        if (strcmp(buf,version)) {
          msg=lives_strdup_printf(
                _("\nThe %s video playback plugin has been updated.\nPlease check your settings in\n Tools|Preferences|Playback|Playback Plugins Advanced\n\n"),
                mainw->vpp->name);
          do_error_dialog(msg);
          lives_free(msg);
          unlink(vpp_file);
          d_print_failed();
          close(fd);
          return;
        }


        lives_read_le(fd,&(mainw->vpp->palette),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->YUV_sampling),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->YUV_clamping),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->YUV_subspace),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->fwidth),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->fheight),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->fixed_fpsd),8,FALSE);
        lives_read_le(fd,&(mainw->vpp->fixed_fps_numer),4,FALSE);
        lives_read_le(fd,&(mainw->vpp->fixed_fps_denom),4,FALSE);

        if (mainw->read_failed) break;

        lives_read_le(fd,&(mainw->vpp->extra_argc),4,FALSE);

        if (mainw->read_failed) break;

        if (vpp->extra_argv!=NULL) {
          for (i=0; vpp->extra_argv[i]!=NULL; i++) {
            lives_free(vpp->extra_argv[i]);
          }
          lives_free(vpp->extra_argv);
        }

        mainw->vpp->extra_argv=(char **)lives_malloc((mainw->vpp->extra_argc+1)*(sizeof(char *)));

        for (i=0; i<mainw->vpp->extra_argc; i++) {
          lives_read_le(fd,&len,4,FALSE);
          if (mainw->read_failed) break;
          mainw->vpp->extra_argv[i]=(char *)lives_malloc(len+1);
          lives_read(fd,mainw->vpp->extra_argv[i],len,FALSE);
          if (mainw->read_failed) break;
          memset((mainw->vpp->extra_argv[i])+len,0,1);
        }

        mainw->vpp->extra_argv[i]=NULL;

        close(fd);
      } while (FALSE);

      if (mainw->read_failed) {
        close(fd);
        retval=do_read_failed_error_s_with_retry(vpp_file,NULL,NULL);
        if (retval==LIVES_RESPONSE_CANCEL) {
          mainw->read_failed=FALSE;
          mainw->vpp=NULL;
          d_print_file_error_failed();
          return;
        }
      }
    }
  } while (retval==LIVES_RESPONSE_RETRY);

  d_print_done();

}


void on_vppa_cancel_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  _vid_playback_plugin *vpp=vppw->plugin;

  lives_widget_destroy(vppw->dialog);
  lives_widget_context_update();
  if (vpp!=NULL&&vpp!=mainw->vpp) {
    // close the temp current vpp
    close_vid_playback_plugin(vpp);
  }

  if (vppw->rfx!=NULL) {
    rfx_free(vppw->rfx);
    lives_free(vppw->rfx);
  }

  lives_free(vppw);

  if (prefsw!=NULL) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
  }
}


void on_vppa_ok_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  const char *fixed_fps=NULL;
  char *cur_pal=NULL;
  const char *tmp;
  int *pal_list,i=0;

  uint64_t xwinid=0;

  _vid_playback_plugin *vpp=vppw->plugin;

  if (vpp==mainw->vpp) {
    if (vppw->spinbuttonw!=NULL) mainw->vpp->fwidth=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonw));
    if (vppw->spinbuttonh!=NULL) mainw->vpp->fheight=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonh));
    if (vppw->fps_entry!=NULL) fixed_fps=lives_entry_get_text(LIVES_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry!=NULL) {
      cur_pal=lives_strdup(lives_entry_get_text(LIVES_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal,' ')>1) {
        char **array=lives_strsplit(cur_pal," ",2);
        char *clamping=lives_strdup(array[1]+1);
        lives_free(cur_pal);
        cur_pal=lives_strdup(array[0]);
        memset(clamping+strlen(clamping)-1,0,1);
        do {
          tmp=weed_yuv_clamping_get_name(i);
          if (tmp!=NULL&&!strcmp(clamping,tmp)) {
            vpp->YUV_clamping=i;
            break;
          }
          i++;
        } while (tmp!=NULL);
        lives_strfreev(array);
        lives_free(clamping);
      }
    }

    if (vppw->fps_entry!=NULL) {
      if (!strlen(fixed_fps)) {
        mainw->vpp->fixed_fpsd=-1.;
        mainw->vpp->fixed_fps_numer=0;
      } else {
        if (get_token_count((char *)fixed_fps,':')>1) {
          char **array=lives_strsplit(fixed_fps,":",2);
          mainw->vpp->fixed_fps_numer=atoi(array[0]);
          mainw->vpp->fixed_fps_denom=atoi(array[1]);
          lives_strfreev(array);
          mainw->vpp->fixed_fpsd=get_ratio_fps((char *)fixed_fps);
        } else {
          mainw->vpp->fixed_fpsd=lives_strtod(fixed_fps,NULL);
          mainw->vpp->fixed_fps_numer=0;
        }
      }
    } else {
      mainw->vpp->fixed_fpsd=-1.;
      mainw->vpp->fixed_fps_numer=0;
    }

    if (mainw->vpp->fixed_fpsd>0.&&(mainw->fixed_fpsd>0.||
                                    (mainw->vpp->set_fps!=NULL&&
                                     !((*mainw->vpp->set_fps)(mainw->vpp->fixed_fpsd))))) {
      do_vpp_fps_error();
      mainw->error=TRUE;
      mainw->vpp->fixed_fpsd=-1.;
      mainw->vpp->fixed_fps_numer=0;
    }

    if (vppw->pal_entry!=NULL) {
      if (vpp->get_palette_list!=NULL&&(pal_list=(*vpp->get_palette_list)())!=NULL) {
        for (i=0; pal_list[i]!=WEED_PALETTE_END; i++) {
          if (!strcmp(cur_pal,weed_palette_get_name(pal_list[i]))) {
            vpp->palette=pal_list[i];
            if (mainw->ext_playback) {
              mainw->ext_keyboard=FALSE;
              if (mainw->vpp->exit_screen!=NULL) {
                (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
              }

#ifdef RT_AUDIO
              stop_audio_stream();
#endif
              mainw->stream_ticks=-1;
              mainw->vpp->palette=pal_list[i];
              if (!(*vpp->set_palette)(vpp->palette)) {
                do_vpp_palette_error();
                mainw->error=TRUE;

              }


              if (prefs->play_monitor!=0) {
                if (mainw->play_window!=NULL) {
                  xwinid=lives_widget_get_xwinid(mainw->play_window,"Unsupported display type for playback plugin");
                  if (xwinid==-1) return;
                }
              }

#ifdef RT_AUDIO
              if (vpp->set_yuv_palette_clamping!=NULL)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

              if (mainw->vpp->audio_codec!=AUDIO_CODEC_NONE&&prefs->stream_audio_out) {
                start_audio_stream();
              }

#endif


              if (vpp->init_screen!=NULL) {
                (*vpp->init_screen)(mainw->pwidth,mainw->pheight,TRUE,xwinid,vpp->extra_argc,vpp->extra_argv);
              }
              if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY&&prefs->play_monitor==0) {
                lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window),TRUE);
                mainw->ext_keyboard=TRUE;
              }
            } else {
              mainw->vpp->palette=pal_list[i];
              if (!(*vpp->set_palette)(vpp->palette)) {
                do_vpp_palette_error();
                mainw->error=TRUE;
              }
              if (vpp->set_yuv_palette_clamping!=NULL)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);
            }
            break;
          }
        }
      }
      lives_free(cur_pal);
    }
    if (vpp->extra_argv!=NULL) {
      for (i=0; vpp->extra_argv[i]!=NULL; i++) lives_free(vpp->extra_argv[i]);
      lives_free(vpp->extra_argv);
      vpp->extra_argv=NULL;
    }
    vpp->extra_argc=0;
    if (vppw->rfx!=NULL) {
      vpp->extra_argv=param_marshall_to_argv(vppw->rfx);
      for (i=0; vpp->extra_argv[i]!=NULL; vpp->extra_argc=++i);
    }
    mainw->write_vpp_file=TRUE;
  } else {
    if (vppw->spinbuttonw!=NULL)
      future_prefs->vpp_fwidth=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonw));
    else future_prefs->vpp_fwidth=-1;
    if (vppw->spinbuttonh!=NULL)
      future_prefs->vpp_fheight=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(vppw->spinbuttonh));
    else future_prefs->vpp_fheight=-1;
    if (vppw->fps_entry!=NULL) fixed_fps=lives_entry_get_text(LIVES_ENTRY(vppw->fps_entry));
    if (vppw->pal_entry!=NULL) {
      cur_pal=lives_strdup(lives_entry_get_text(LIVES_ENTRY(vppw->pal_entry)));

      if (get_token_count(cur_pal,' ')>1) {
        char **array=lives_strsplit(cur_pal," ",2);
        char *clamping=lives_strdup(array[1]+1);
        lives_free(cur_pal);
        cur_pal=lives_strdup(array[0]);
        memset(clamping+strlen(clamping)-1,0,1);
        do {
          tmp=weed_yuv_clamping_get_name(i);
          if (tmp!=NULL&&!strcmp(clamping,tmp)) {
            future_prefs->vpp_YUV_clamping=i;
            break;
          }
          i++;
        } while (tmp!=NULL);
        lives_strfreev(array);
        lives_free(clamping);
      }
    }

    if (fixed_fps!=NULL) {
      if (get_token_count((char *)fixed_fps,':')>1) {
        char **array=lives_strsplit(fixed_fps,":",2);
        future_prefs->vpp_fixed_fps_numer=atoi(array[0]);
        future_prefs->vpp_fixed_fps_denom=atoi(array[1]);
        lives_strfreev(array);
        future_prefs->vpp_fixed_fpsd=get_ratio_fps((char *)fixed_fps);
      } else {
        future_prefs->vpp_fixed_fpsd=lives_strtod(fixed_fps,NULL);
        future_prefs->vpp_fixed_fps_numer=0;
      }
    } else {
      future_prefs->vpp_fixed_fpsd=-1.;
      future_prefs->vpp_fixed_fps_numer=0;
    }

    if (cur_pal!=NULL) {
      if (vpp->get_palette_list!=NULL&&(pal_list=(*vpp->get_palette_list)())!=NULL) {
        for (i=0; pal_list[i]!=WEED_PALETTE_END; i++) {
          if (!strcmp(cur_pal,weed_palette_get_name(pal_list[i]))) {
            future_prefs->vpp_palette=pal_list[i];
            break;
          }
        }
      }
      lives_free(cur_pal);
    } else future_prefs->vpp_palette=WEED_PALETTE_END;

    if (future_prefs->vpp_argv!=NULL) {
      for (i=0; future_prefs->vpp_argv[i]!=NULL; i++) lives_free(future_prefs->vpp_argv[i]);
      lives_free(future_prefs->vpp_argv);
      future_prefs->vpp_argv=NULL;
    }

    future_prefs->vpp_argc=0;
    if (vppw->rfx!=NULL) {
      future_prefs->vpp_argv=param_marshall_to_argv(vppw->rfx);
      if (future_prefs->vpp_argv!=NULL) {
        for (i=0; future_prefs->vpp_argv[i]!=NULL; future_prefs->vpp_argc=++i);
      }
    } else {
      future_prefs->vpp_argv=vpp->extra_argv;
      vpp->extra_argv=NULL;
      vpp->extra_argc=0;
    }
  }
  if (button!=NULL&&!mainw->error) on_vppa_cancel_clicked(button,user_data);
  if (button!=NULL) mainw->error=FALSE;
}


void on_vppa_save_clicked(LiVESButton *button, livespointer user_data) {
  _vppaw *vppw=(_vppaw *)user_data;
  _vid_playback_plugin *vpp=vppw->plugin;
  char *save_file;

  // apply
  mainw->error=FALSE;
  on_vppa_ok_clicked(NULL,user_data);
  if (mainw->error) {
    mainw->error=FALSE;
    return;
  }

  // get filename
  save_file=choose_file(NULL,NULL,NULL,LIVES_FILE_CHOOSER_ACTION_SAVE,NULL,NULL);
  if (save_file==NULL) return;

  // save
  d_print(_("Saving playback plugin defaults to %s..."),save_file);
  save_vpp_defaults(vpp, save_file);
  d_print_done();
  lives_free(save_file);

}





_vppaw *on_vpp_advanced_clicked(LiVESButton *button, livespointer user_data) {
  LiVESWidget *dialog_vbox;
  LiVESWidget *hbox;
  LiVESWidget *label;
  LiVESWidget *combo;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;
  LiVESWidget *savebutton;

  LiVESAccelGroup *accel_group;

  _vppaw *vppa;

  _vid_playback_plugin *tmpvpp;

  int *pal_list;

  LiVESList *fps_list_strings=NULL;
  LiVESList *pal_list_strings=NULL;

  const char *string;
  const char *pversion;
  const char *desc;
  const char *fps_list;

  char *title;
  char *tmp,*tmp2;

  char *ctext=NULL;

  char *com;

  // TODO - set default values from tmpvpp

  if (strlen(future_prefs->vpp_name)) {
    if ((tmpvpp=open_vid_playback_plugin(future_prefs->vpp_name, FALSE))==NULL) return NULL;
  } else {
    if (mainw->vpp==NULL) return NULL;
    tmpvpp=mainw->vpp;
  }

  vppa=(_vppaw *)(lives_malloc(sizeof(_vppaw)));

  vppa->plugin=tmpvpp;

  vppa->spinbuttonh=vppa->spinbuttonw=NULL;
  vppa->pal_entry=vppa->fps_entry=NULL;

  pversion=(tmpvpp->version)();

  title=lives_strdup_printf("LiVES: - %s",pversion);

  vppa->dialog = lives_standard_dialog_new(title,FALSE,DEF_DIALOG_WIDTH,DEF_DIALOG_HEIGHT);
  lives_free(title);

  accel_group = LIVES_ACCEL_GROUP(lives_accel_group_new());
  lives_window_add_accel_group(LIVES_WINDOW(vppa->dialog), accel_group);

  if (prefs->show_gui) {
    if (prefsw!=NULL) lives_window_set_transient_for(LIVES_WINDOW(vppa->dialog),LIVES_WINDOW(prefsw->prefs_dialog));
    else {
      if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(vppa->dialog),LIVES_WINDOW(mainw->LiVES));
      else lives_window_set_transient_for(LIVES_WINDOW(vppa->dialog),LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(vppa->dialog));

  // the filling...
  if (tmpvpp->get_description!=NULL) {
    desc=(tmpvpp->get_description)();
    if (desc!=NULL) {
      label = lives_standard_label_new(desc);
      lives_box_pack_start(LIVES_BOX(dialog_vbox), label, FALSE, FALSE, widget_opts.packing_height);
    }
  }


  if (tmpvpp->get_fps_list!=NULL&&(fps_list=(*tmpvpp->get_fps_list)(tmpvpp->palette))!=NULL) {
    int nfps,i;
    char **array=lives_strsplit(fps_list,"|",-1);

    nfps=get_token_count((char *)fps_list,'|');
    for (i=0; i<nfps; i++) {
      if (strlen(array[i])&&strcmp(array[i],"\n")) {
        if (get_token_count(array[i],':')==0) {
          fps_list_strings=lives_list_append(fps_list_strings, remove_trailing_zeroes(lives_strtod(array[i],NULL)));
        } else fps_list_strings=lives_list_append(fps_list_strings,lives_strdup(array[i]));
      }
    }

    // fps
    combo = lives_standard_combo_new((tmp=lives_strdup(_("_FPS"))),TRUE,fps_list_strings,
                                     LIVES_BOX(dialog_vbox),(tmp2=lives_strdup(_("Fixed framerate for plugin.\n"))));

    lives_free(tmp);
    lives_free(tmp2);
    vppa->fps_entry=lives_combo_get_entry(LIVES_COMBO(combo));
    lives_entry_set_width_chars(LIVES_ENTRY(lives_combo_get_entry(LIVES_COMBO(combo))), 14);


    lives_list_free_strings(fps_list_strings);
    lives_list_free(fps_list_strings);
    fps_list_strings=NULL;
    lives_strfreev(array);

    if (tmpvpp->fixed_fps_numer>0) {
      char *tmp=lives_strdup_printf("%d:%d",tmpvpp->fixed_fps_numer,tmpvpp->fixed_fps_denom);
      lives_entry_set_text(LIVES_ENTRY(vppa->fps_entry),tmp);
      lives_free(tmp);
    } else {
      char *tmp=remove_trailing_zeroes(tmpvpp->fixed_fpsd);
      lives_entry_set_text(LIVES_ENTRY(vppa->fps_entry),tmp);
      lives_free(tmp);
    }
  }


  // frame size

  if (!(tmpvpp->capabilities&VPP_LOCAL_DISPLAY)) {
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), hbox, FALSE, FALSE, widget_opts.packing_height);

    add_fill_to_box(LIVES_BOX(hbox));

    vppa->spinbuttonw = lives_standard_spin_button_new(_("_Width"),TRUE,
                        tmpvpp->fwidth>0?tmpvpp->fwidth:DEF_VPP_HSIZE,
                        4., MAX_FRAME_WIDTH, 4., 4., 0, LIVES_BOX(hbox),NULL);

    add_fill_to_box(LIVES_BOX(hbox));

    vppa->spinbuttonh = lives_standard_spin_button_new(_("_Height"),TRUE,
                        tmpvpp->fheight>0?tmpvpp->fheight:DEF_VPP_VSIZE,
                        4., MAX_FRAME_HEIGHT, 4., 4., 0, LIVES_BOX(hbox),NULL);

    add_fill_to_box(LIVES_BOX(hbox));
  }


  // palette

  if (tmpvpp->get_palette_list!=NULL&&(pal_list=(*tmpvpp->get_palette_list)())!=NULL) {
    int i;

    for (i=0; pal_list[i]!=WEED_PALETTE_END; i++) {
      int j=0;
      string=weed_palette_get_name(pal_list[i]);
      if (weed_palette_is_yuv_palette(pal_list[i])&&tmpvpp->get_yuv_palette_clamping!=NULL) {
        int *clampings=(*tmpvpp->get_yuv_palette_clamping)(pal_list[i]);
        while (clampings[j]!=-1) {
          char *string2=lives_strdup_printf("%s (%s)",string,weed_yuv_clamping_get_name(clampings[j]));
          pal_list_strings=lives_list_append(pal_list_strings, string2);
          j++;
        }
      }
      if (j==0) {
        pal_list_strings=lives_list_append(pal_list_strings, lives_strdup(string));
      }
    }

    combo = lives_standard_combo_new((tmp=lives_strdup(_("_Colourspace"))),TRUE,pal_list_strings,
                                     LIVES_BOX(dialog_vbox),tmp2=lives_strdup(_("Colourspace input to the plugin.\n")));
    lives_free(tmp);
    lives_free(tmp2);
    vppa->pal_entry=lives_combo_get_entry(LIVES_COMBO(combo));

    if (tmpvpp->get_yuv_palette_clamping!=NULL&&weed_palette_is_yuv_palette(tmpvpp->palette)) {
      int *clampings=tmpvpp->get_yuv_palette_clamping(tmpvpp->palette);
      if (clampings[0]!=-1)
        ctext=lives_strdup_printf("%s (%s)",weed_palette_get_name(tmpvpp->palette),
                                  weed_yuv_clamping_get_name(tmpvpp->YUV_clamping));
    }
    if (ctext==NULL) ctext=lives_strdup(weed_palette_get_name(tmpvpp->palette));
    lives_entry_set_text(LIVES_ENTRY(vppa->pal_entry),ctext);
    lives_free(ctext);
    lives_list_free_strings(pal_list_strings);
    lives_list_free(pal_list_strings);
  }

  // extra params

  if (tmpvpp->get_init_rfx!=NULL) {
    LiVESWidget *vbox=lives_vbox_new(FALSE, 0);
    LiVESWidget *scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V/2, vbox);
    lives_box_pack_start(LIVES_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);

#ifndef IS_MINGW
    com=lives_strdup_printf("%s -e \"%s\"",capable->echo_cmd,(*tmpvpp->get_init_rfx)());
#else
    com=lives_strdup_printf("echo.exe -e \"%s\"",(*tmpvpp->get_init_rfx)());
#endif
    plugin_run_param_window(com,LIVES_VBOX(vbox),&(vppa->rfx));
    lives_free(com);
    if (tmpvpp->extra_argv!=NULL&&tmpvpp->extra_argc>0) {
      // update with defaults
      LiVESList *plist=argv_to_marshalled_list(vppa->rfx,tmpvpp->extra_argc,tmpvpp->extra_argv);
      param_demarshall(vppa->rfx,plist,FALSE,FALSE); // set defaults
      param_demarshall(vppa->rfx,plist,FALSE,TRUE); // update widgets
      lives_list_free_strings(plist);
      lives_list_free(plist);
    }
  } else {
    vppa->rfx=NULL;
  }


  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(vppa->dialog), cancelbutton, LIVES_RESPONSE_CANCEL);
  lives_widget_set_can_focus(cancelbutton,TRUE);

  lives_widget_add_accelerator(cancelbutton, LIVES_WIDGET_CLICKED_SIGNAL, accel_group,
                               LIVES_KEY_Escape, (LiVESXModifierType)0, (LiVESAccelFlags)0);


  savebutton = lives_button_new_from_stock(LIVES_STOCK_SAVE_AS,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(vppa->dialog), savebutton, 1);
  lives_widget_set_can_focus(savebutton,TRUE);
  lives_widget_set_tooltip_text(savebutton, _("Save settings to an alternate file.\n"));

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(vppa->dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vppa_cancel_clicked),
                       vppa);

  lives_signal_connect(LIVES_GUI_OBJECT(savebutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vppa_save_clicked),
                       vppa);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(on_vppa_ok_clicked),
                       vppa);



  lives_widget_show_all(vppa->dialog);
  lives_window_present(LIVES_WINDOW(vppa->dialog));
  lives_xwindow_raise(lives_widget_get_xwindow(vppa->dialog));

  return vppa;
}


void close_vid_playback_plugin(_vid_playback_plugin *vpp) {
  register int i;

  if (vpp!=NULL) {
    if (vpp==mainw->vpp) {
      mainw->ext_keyboard=FALSE;
      if (mainw->ext_playback) {
        if (mainw->vpp->exit_screen!=NULL)
          (*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
#ifdef RT_AUDIO
        stop_audio_stream();
#endif
        if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)
          if (mainw->play_window!=NULL&&prefs->play_monitor==0)
            lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window),FALSE);
      }
      mainw->stream_ticks=-1;
      mainw->vpp=NULL;
    }
    if (vpp->module_unload!=NULL)(vpp->module_unload)();
    dlclose(vpp->handle);

    if (vpp->extra_argv!=NULL) {
      for (i=0; vpp->extra_argv[i]!=NULL; i++) {
        lives_free(vpp->extra_argv[i]);
      }
      lives_free(vpp->extra_argv);
    }

    for (i=0; i<vpp->num_play_params+vpp->num_alpha_chans; i++) {
      weed_plant_free(vpp->play_params[i]);
    }

    if (vpp->play_params!=NULL) lives_free(vpp->play_params);

    lives_free(vpp);
  }
}


const weed_plant_t *pp_get_param(weed_plant_t **pparams, int idx) {
  register int i=0;
  while (pparams[i]!=NULL) {
    if (WEED_PLANT_IS_PARAMETER(pparams[i])) {
      if (--idx<0) return pparams[i];
    }
    i++;
  }
  return NULL;
}


const weed_plant_t *pp_get_chan(weed_plant_t **pparams, int idx) {
  register int i=0;
  while (pparams[i]!=NULL) {
    if (WEED_PLANT_IS_CHANNEL(pparams[i])) {
      if (--idx<0) return pparams[i];
    }
    i++;
  }
  return NULL;
}



_vid_playback_plugin *open_vid_playback_plugin(const char *name, boolean in_use) {
  // this is called on startup or when the user selects a new playback plugin

  // if in_use is TRUE, it is our active vpp

  // TODO - if in_use, get fixed_fps,fwidth,fheight,palette,argc and argv from a file
  // TODO - dirsep

#ifndef IS_MINGW
  char *plugname=lives_strdup_printf("%s%s%s/%s.so",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_VID_PLAYBACK,name);
#else
  char *plugname=lives_strdup_printf("%s%s%s/%s.dll",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_VID_PLAYBACK,name);
#endif
  void *handle=dlopen(plugname,RTLD_LAZY);
  boolean OK=TRUE;
  char *msg,*tmp;
  char **array;
  const char *fps_list;
  const char *pl_error;
  int i;
  int *palette_list;
  _vid_playback_plugin *vpp;

  if (handle==NULL) {
    char *msg=lives_strdup_printf(_("\n\nFailed to open playback plugin %s\nError was %s\n"),plugname,dlerror());
    if (prefsw!=NULL) do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):
          LIVES_WINDOW(mainw->LiVES));
    else do_error_dialog(msg);
    lives_free(msg);
    lives_free(plugname);
    return NULL;
  }


  vpp=(_vid_playback_plugin *) lives_malloc(sizeof(_vid_playback_plugin));

  vpp->play_paramtmpls=NULL;
  vpp->play_params=NULL;
  vpp->alpha_chans=NULL;
  vpp->num_play_params=vpp->num_alpha_chans=0;
  vpp->extra_argv=NULL;

  if ((vpp->module_check_init=(const char* ( *)())dlsym(handle,"module_check_init"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->version=(const char* ( *)())dlsym(handle,"version"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_palette_list=(int* ( *)())dlsym(handle,"get_palette_list"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->set_palette=(boolean( *)(int))dlsym(handle,"set_palette"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_capabilities=(uint64_t ( *)(int))dlsym(handle,"get_capabilities"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->render_frame=(boolean( *)(int, int, int64_t, void **, void **, weed_plant_t **))
                         dlsym(handle,"render_frame"))==NULL) {
    OK=FALSE;
  }
  if ((vpp->get_fps_list=(const char* ( *)(int))dlsym(handle,"get_fps_list"))!=NULL) {
    if ((vpp->set_fps=(boolean( *)(double))dlsym(handle,"set_fps"))==NULL) {
      OK=FALSE;
    }
  }


  if (!OK) {
    char *msg=lives_strdup_printf
              (_("\n\nPlayback module %s\nis missing a mandatory function.\nUnable to use it.\n"),plugname);
    set_pref("vid_playback_plugin","none");
    do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):
                                         LIVES_WINDOW(mainw->LiVES));
    lives_free(msg);
    dlclose(handle);
    lives_free(vpp);
    vpp=NULL;
    lives_free(plugname);
    return NULL;
  }

  if ((pl_error=(*vpp->module_check_init)())!=NULL) {
    msg=lives_strdup_printf(_("Video playback plugin failed to initialise.\nError was: %s\n"),pl_error);
    do_error_dialog_with_check_transient(msg,TRUE,0,prefsw!=NULL?LIVES_WINDOW(prefsw->prefs_dialog):
                                         LIVES_WINDOW(mainw->LiVES));
    lives_free(msg);
    dlclose(handle);
    lives_free(vpp);
    vpp=NULL;
    lives_free(plugname);
    return NULL;
  }

  // now check for optional functions
  vpp->get_description=(const char* ( *)())dlsym(handle,"get_description");
  vpp->get_init_rfx=(const char* ( *)())dlsym(handle,"get_init_rfx");

  vpp->get_play_params=(const weed_plant_t **( *)(weed_bootstrap_f))dlsym(handle,"get_play_params");

  vpp->get_yuv_palette_clamping=(int* ( *)(int))dlsym(handle,"get_yuv_palette_clamping");
  vpp->set_yuv_palette_clamping=(int ( *)(int))dlsym(handle,"set_yuv_palette_clamping");
  vpp->send_keycodes=(boolean( *)(plugin_keyfunc))dlsym(handle,"send_keycodes");
  vpp->get_audio_fmts=(int* ( *)())dlsym(handle,"get_audio_fmts");
  vpp->init_screen=(boolean( *)(int, int, boolean, uint64_t, int, char **))dlsym(handle,"init_screen");
  vpp->exit_screen=(void ( *)(uint16_t, uint16_t))dlsym(handle,"exit_screen");
  vpp->module_unload=(void ( *)())dlsym(handle,"module_unload");

  vpp->YUV_sampling=0;
  vpp->YUV_subspace=0;

  palette_list=(*vpp->get_palette_list)();

  if (future_prefs->vpp_argv!=NULL) {
    vpp->palette=future_prefs->vpp_palette;
    vpp->YUV_clamping=future_prefs->vpp_YUV_clamping;
  } else {
    if (!in_use&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
      vpp->palette=mainw->vpp->palette;
      vpp->YUV_clamping=mainw->vpp->YUV_clamping;
    } else {
      vpp->palette=palette_list[0];
      vpp->YUV_clamping=-1;
    }
  }

  vpp->audio_codec=AUDIO_CODEC_NONE;
  vpp->capabilities=(*vpp->get_capabilities)(vpp->palette);

  if (vpp->capabilities&VPP_CAN_RESIZE) {
    vpp->fwidth=vpp->fheight=-1;
  } else {
    vpp->fwidth=vpp->fheight=0;
  }
  if (future_prefs->vpp_argv!=NULL) {
    vpp->fwidth=future_prefs->vpp_fwidth;
    vpp->fheight=future_prefs->vpp_fheight;
  } else if (!in_use&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
    vpp->fwidth=mainw->vpp->fwidth;
    vpp->fheight=mainw->vpp->fheight;
  }

  vpp->fixed_fpsd=-1.;
  vpp->fixed_fps_numer=0;

  if (future_prefs->vpp_argv!=NULL) {
    vpp->fixed_fpsd=future_prefs->vpp_fixed_fpsd;
    vpp->fixed_fps_numer=future_prefs->vpp_fixed_fps_numer;
    vpp->fixed_fps_denom=future_prefs->vpp_fixed_fps_denom;
  } else if (!in_use&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
    vpp->fixed_fpsd=mainw->vpp->fixed_fpsd;
    vpp->fixed_fps_numer=mainw->vpp->fixed_fps_numer;
    vpp->fixed_fps_denom=mainw->vpp->fixed_fps_denom;
  }

  vpp->handle=handle;
  lives_snprintf(vpp->name,256,"%s",name);

  if (future_prefs->vpp_argv!=NULL) {
    vpp->extra_argc=future_prefs->vpp_argc;
    vpp->extra_argv=(char **)lives_malloc((vpp->extra_argc+1)*(sizeof(char *)));
    for (i=0; i<=vpp->extra_argc; i++) vpp->extra_argv[i]=lives_strdup(future_prefs->vpp_argv[i]);
  } else {
    if (!in_use&&mainw->vpp!=NULL&&!(strcmp(name,mainw->vpp->name))) {
      vpp->extra_argc=mainw->vpp->extra_argc;
      vpp->extra_argv=(char **)lives_malloc((mainw->vpp->extra_argc+1)*(sizeof(char *)));
      for (i=0; i<=vpp->extra_argc; i++) vpp->extra_argv[i]=lives_strdup(mainw->vpp->extra_argv[i]);
    } else {
      vpp->extra_argc=0;
      vpp->extra_argv=(char **)lives_malloc(sizeof(char *));
      vpp->extra_argv[0]=NULL;
    }
  }
  // see if plugin is using fixed fps

  if (vpp->fixed_fpsd<=0.&&vpp->get_fps_list!=NULL) {
    // fixed fps

    if ((fps_list=(*vpp->get_fps_list)(vpp->palette))!=NULL) {
      array=lives_strsplit(fps_list,"|",-1);
      if (get_token_count(array[0],':')>1) {
        char **array2=lives_strsplit(array[0],":",2);
        vpp->fixed_fps_numer=atoi(array2[0]);
        vpp->fixed_fps_denom=atoi(array2[1]);
        lives_strfreev(array2);
        vpp->fixed_fpsd=get_ratio_fps(array[0]);
      } else {
        vpp->fixed_fpsd=lives_strtod(array[0],NULL);
        vpp->fixed_fps_numer=0;
      }
      lives_strfreev(array);
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
  if (prefsw!=NULL) {
    prefsw_set_astream_settings(vpp);
    prefsw_set_rec_after_settings(vpp);
  }

  if (!in_use) return vpp;

  if (!mainw->is_ready) {
    double fixed_fpsd=vpp->fixed_fpsd;
    int fwidth=vpp->fwidth;
    int fheight=vpp->fheight;

    mainw->vpp=vpp;
    load_vpp_defaults(vpp, mainw->vpp_defs_file);
    if (fixed_fpsd<0.) vpp->fixed_fpsd=fixed_fpsd;
    if (fwidth<0) vpp->fwidth=fwidth;
    if (fheight<0) vpp->fheight=fheight;
  }

  if (!(*vpp->set_palette)(vpp->palette)) {
    do_vpp_palette_error();
    close_vid_playback_plugin(vpp);
    lives_free(plugname);
    return NULL;
  }

  if (vpp->set_yuv_palette_clamping!=NULL)(*vpp->set_yuv_palette_clamping)(vpp->YUV_clamping);

  if (vpp->get_fps_list!=NULL) {
    if (mainw->fixed_fpsd>0.||(vpp->fixed_fpsd>0.&&vpp->set_fps!=NULL&&
                               !((*vpp->set_fps)(vpp->fixed_fpsd)))) {
      do_vpp_fps_error();
      vpp->fixed_fpsd=-1.;
      vpp->fixed_fps_numer=0;
    }
  }

  // get the play parameters (and alpha channels) if any and convert to weed params
  if (vpp->get_play_params!=NULL) {
    vpp->play_paramtmpls=(*vpp->get_play_params)(weed_bootstrap_func);
  }

  // create vpp->play_params
  if (vpp->play_paramtmpls!=NULL) {
    weed_plant_t *ptmpl;
    for (i=0; (ptmpl=(weed_plant_t *)vpp->play_paramtmpls[i])!=NULL; i++) {
      vpp->play_params=(weed_plant_t **)lives_realloc(vpp->play_params,(i+2)*sizeof(weed_plant_t *));
      if (WEED_PLANT_IS_PARAMETER_TEMPLATE(ptmpl)) {
        // is param template, create a param
        vpp->play_params[i]=weed_plant_new(WEED_PLANT_PARAMETER);
        weed_leaf_copy(vpp->play_params[i],"value",ptmpl,"default");
        weed_set_plantptr_value(vpp->play_params[i],"template",ptmpl);
        vpp->num_play_params++;
      } else {
        // must be an alpha channel
        vpp->play_params[i]=weed_plant_new(WEED_PLANT_CHANNEL);
        weed_set_plantptr_value(vpp->play_params[i],"template",ptmpl);
        vpp->num_alpha_chans++;
      }
    }
    vpp->play_params[i]=NULL;
  }

  if (vpp->send_keycodes==NULL&&vpp->capabilities&VPP_LOCAL_DISPLAY) {
    d_print
    (_("\nWarning ! Video playback plugin will not send key presses. Keyboard may be disabled during plugin use !\n"));
  }

  cached_key=cached_mod=0;

  d_print(_("*** Using %s plugin for fs playback, agreed to use palette type %d ( %s ). ***\n"),name,
          vpp->palette,(tmp=weed_palette_get_name_full(vpp->palette,vpp->YUV_clamping,
                            WEED_YUV_SUBSPACE_YCBCR)));
  lives_free(tmp);
  lives_free(plugname);

  while (mainw->noswitch) {
    lives_usleep(prefs->sleep_time);
  }

  if (mainw->is_ready&&in_use&&mainw->vpp!=NULL) {
    close_vid_playback_plugin(mainw->vpp);
  }

  return vpp;
}


void vid_playback_plugin_exit(void) {
  // external plugin
  if (mainw->ext_playback) {
    mainw->ext_keyboard=FALSE;
    if (mainw->vpp->exit_screen!=NULL)(*mainw->vpp->exit_screen)(mainw->ptr_x,mainw->ptr_y);
#ifdef RT_AUDIO
    stop_audio_stream();
#endif
    mainw->ext_playback=FALSE;
    if (mainw->vpp->capabilities&VPP_LOCAL_DISPLAY)
      if (mainw->play_window!=NULL&&prefs->play_monitor==0)
        lives_window_set_keep_below(LIVES_WINDOW(mainw->play_window),FALSE);
  }
  mainw->stream_ticks=-1;

  if (mainw->playing_file>-1&&mainw->fs&&mainw->sep_win) lives_window_fullscreen(LIVES_WINDOW(mainw->play_window));
  if (mainw->play_window!=NULL)
    lives_window_set_title(LIVES_WINDOW(mainw->play_window),_("LiVES: - Play Window"));
}


int64_t get_best_audio(_vid_playback_plugin *vpp) {
  // find best audio from video plugin list, matching with audiostream plugins

  // i.e. cross-check video list with astreamer list

  int *fmts,*sfmts;
  int ret=AUDIO_CODEC_NONE;
  int i,j=0,nfmts;
  size_t rlen;
  char *astreamer,*com;
  char buf[1024];
  char **array;
  FILE *rfile;

  if (vpp!=NULL&&vpp->get_audio_fmts!=NULL) {
    fmts=(*vpp->get_audio_fmts)(); // const, so do not free()

    // make audiostream plugin name
    astreamer=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_AUDIO_STREAM,"audiostreamer.pl",NULL);

    // create sfmts array and nfmts

    com=lives_strdup_printf("\"%s\" get_formats",astreamer);

    rfile=popen(com,"r");
    rlen=fread(buf,1,1023,rfile);
    pclose(rfile);
    memset(buf+rlen,0,1);
    lives_free(com);

    nfmts=get_token_count(buf,'|');
    array=lives_strsplit(buf,"|",nfmts);
    sfmts=(int *)lives_malloc(nfmts*sizint);

    for (i=0; i<nfmts; i++) {
      if (array[i]!=NULL&&strlen(array[i])>0) sfmts[j++]=atoi(array[i]);
    }

    nfmts=j;
    lives_strfreev(array);

    for (i=0; fmts[i]!=-1; i++) {
      // traverse video list and see if audiostreamer supports each one
      if (int_array_contains_value(sfmts,nfmts,fmts[i])) {

        com=lives_strdup_printf("\"%s\" check %d",astreamer,fmts[i]);

        rfile=popen(com,"r");
        if (!rfile) {
          // command failed
          do_system_failed_error(com,0,NULL);
          lives_free(astreamer);
          lives_free(com);
          lives_free(sfmts);
          return ret;
        }
        rlen=fread(buf,1,1023,rfile);
        pclose(rfile);
        memset(buf+rlen,0,1);
        lives_free(com);

        if (strlen(buf)>0) {
          if (i==0&&prefsw!=NULL) {
            do_error_dialog_with_check_transient
            (buf,TRUE,0,LIVES_WINDOW(prefsw->prefs_dialog));
            d_print(_("Audio stream unable to use preferred format '%s'\n"),anames[fmts[i]]);
          }
          continue;
        }

        if (i>0&&prefsw!=NULL) {
          d_print(_("Using format '%s' instead.\n"),anames[fmts[i]]);
        }
        ret=fmts[i];
        break;
      }
    }

    if (fmts[i]==-1) {
      //none suitable, stick with first
      for (i=0; fmts[i]!=-1; i++) {
        // traverse video list and see if audiostreamer supports each one
        if (int_array_contains_value(sfmts,nfmts,fmts[i])) {
          ret=fmts[i];
          break;
        }
      }
    }

    lives_free(sfmts);
    lives_free(astreamer);
  }

  return ret;
}


///////////////////////
// encoder plugins



void do_plugin_encoder_error(const char *plugin_name) {
  char *msg,*tmp;

  if (plugin_name==NULL) {
    msg=lives_strdup_printf(
          _("LiVES was unable to find its encoder plugins. Please make sure you have the plugins installed in\n%s%s%s\nor change the value of <lib_dir> in %s\n"),
          prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS,(tmp=lives_filename_to_utf8(capable->rcfile,-1,NULL,NULL,NULL)));
    lives_free(tmp);
    if (rdet!=NULL) do_error_dialog_with_check_transient(msg,FALSE,0,LIVES_WINDOW(rdet->dialog));
    else do_error_dialog(msg);
    lives_free(msg);
    return;
  }

  msg=lives_strdup_printf(
        _("LiVES did not receive a response from the encoder plugin called '%s'.\nPlease make sure you have that plugin installed correctly in\n%s%s%s\nor switch to another plugin using Tools|Preferences|Encoding\n"),
        plugin_name,prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_ENCODERS);
  do_blocking_error_dialog(msg);
  lives_free(msg);
}


boolean check_encoder_restrictions(boolean get_extension, boolean user_audio, boolean save_all) {
  char **checks;
  char **array=NULL;
  char **array2;
  int pieces,numtok;
  boolean calc_aspect=FALSE;
  char aspect_buffer[512];
  int hblock=2,vblock=2;
  int i,r,val;
  LiVESList *ofmt_all=NULL;
  boolean sizer=FALSE;

  // for auto resizing/resampling
  double best_fps=0.;
  int best_arate=0;
  int width,owidth;
  int height,oheight;

  double best_fps_delta=0.;
  int best_arate_delta=0;
  boolean allow_aspect_override=FALSE;

  int best_fps_num=0,best_fps_denom=0;
  double fps;
  int arate,achans,asampsize,asigned=0;

  boolean swap_endian=FALSE;

  if (rdet==NULL) {
    width=owidth=cfile->hsize;
    height=oheight=cfile->vsize;
    fps=cfile->fps;
  } else {
    width=owidth=rdet->width;
    height=oheight=rdet->height;
    fps=rdet->fps;
    rdet->suggestion_followed=FALSE;
  }

  if (mainw->osc_auto) {
    if (mainw->osc_enc_width>0) {
      width=mainw->osc_enc_width;
      height=mainw->osc_enc_height;
    }
    if (mainw->osc_enc_fps!=0.) fps=mainw->osc_enc_fps;
  }

  // TODO - allow lists for size
  lives_snprintf(prefs->encoder.of_restrict,5,"none");
  if (!((ofmt_all=plugin_request_by_line(PLUGIN_ENCODERS,prefs->encoder.name,"get_formats"))==NULL)) {
    // get any restrictions for the current format
    for (i=0; i<lives_list_length(ofmt_all); i++) {
      if ((numtok=get_token_count((char *)lives_list_nth_data(ofmt_all,i),'|'))>2) {
        array=lives_strsplit((char *)lives_list_nth_data(ofmt_all,i),"|",-1);
        if (!strcmp(array[0],prefs->encoder.of_name)) {
          if (numtok>4) {
            lives_snprintf(prefs->encoder.of_def_ext,16,"%s",array[4]);
          } else {
            memset(prefs->encoder.of_def_ext,0,1);
          }
          if (numtok>3) {
            lives_snprintf(prefs->encoder.of_restrict,128,"%s",array[3]);
          } else {
            lives_snprintf(prefs->encoder.of_restrict,128,"none");
          }
          prefs->encoder.of_allowed_acodecs=atoi(array[2]);
          lives_list_free_strings(ofmt_all);
          lives_list_free(ofmt_all);
          lives_strfreev(array);
          break;
        }
        lives_strfreev(array);
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
  } else {
    arate=rdet->arate;
    achans=rdet->achans;
    asampsize=rdet->asamps;
  }

  // audio endianness check - what should we do for big-endian machines ?
  if (((mainw->save_with_sound||rdet!=NULL)&&(resaudw==NULL||resaudw->aud_checkbutton==NULL||
       lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton))))
      &&prefs->encoder.audio_codec!=AUDIO_CODEC_NONE&&(arate*achans*asampsize)) {
    if (rdet!=NULL&&!rdet->is_encoding) {
      if (mainw->endian!=AFORM_BIG_ENDIAN && (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend))))
        swap_endian=TRUE;
    } else {
      if (mainw->endian!=AFORM_BIG_ENDIAN && (cfile->signed_endian&AFORM_BIG_ENDIAN)) swap_endian=TRUE;
      //if (mainw->endian==AFORM_BIG_ENDIAN && (cfile->signed_endian&AFORM_BIG_ENDIAN)) swap_endian=TRUE; // needs test
    }
  }


  if (strlen(prefs->encoder.of_restrict)>0) {

    pieces=get_token_count(prefs->encoder.of_restrict,',');
    checks=lives_strsplit(prefs->encoder.of_restrict,",",pieces);


    for (r=0; r<pieces; r++) {
      // check each restriction in turn

      if (!strncmp(checks[r],"fps=",4)) {
        double allowed_fps;
        int mbest_num=0,mbest_denom=0;
        int numparts;
        char *fixer;

        best_fps_delta=1000000000.;
        array=lives_strsplit(checks[r],"=",2);
        numtok=get_token_count(array[1],';');
        array2=lives_strsplit(array[1],";",numtok);
        for (i=0; i<numtok; i++) {
          mbest_num=mbest_denom=0;
          if ((numparts=get_token_count(array2[i],':'))>1) {
            char **array3=lives_strsplit(array2[i],":",2);
            mbest_num=atoi(array3[0]);
            mbest_denom=atoi(array3[1]);
            lives_strfreev(array3);
            if (mbest_denom==0) continue;
            allowed_fps=(mbest_num*1.)/(mbest_denom*1.);
          } else allowed_fps=lives_strtod(array2[i],NULL);

          // convert to 8dp
          fixer=lives_strdup_printf("%.8f %.8f",allowed_fps,fps);
          lives_free(fixer);

          if (allowed_fps>=fps) {
            if (allowed_fps-fps<best_fps_delta) {
              best_fps_delta=allowed_fps-fps;
              if (mbest_denom>0) {
                best_fps_num=mbest_num;
                best_fps_denom=mbest_denom;
                best_fps=0.;
                if (rdet==NULL) cfile->ratio_fps=TRUE;
                else rdet->ratio_fps=TRUE;
              } else {
                best_fps_num=best_fps_denom=0;
                best_fps=allowed_fps;
                if (rdet==NULL) cfile->ratio_fps=FALSE;
                else rdet->ratio_fps=FALSE;
              }
            }
          } else if ((best_fps_denom==0&&allowed_fps>best_fps)||(best_fps_denom>0&&allowed_fps>(best_fps_num*1.)/
                     (best_fps_denom*1.))) {
            best_fps_delta=fps-allowed_fps;
            if (mbest_denom>0) {
              best_fps_num=mbest_num;
              best_fps_denom=mbest_denom;
              best_fps=0.;
              if (rdet==NULL) cfile->ratio_fps=TRUE;
              else rdet->ratio_fps=TRUE;
            } else {
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
        lives_strfreev(array);
        lives_strfreev(array2);
        continue;
      }

      if (!strncmp(checks[r],"size=",5)) {
        // TODO - allow list for size
        array=lives_strsplit(checks[r],"=",2);
        array2=lives_strsplit(array[1],"x",2);
        width=atoi(array2[0]);
        height=atoi(array2[1]);
        lives_strfreev(array2);
        lives_strfreev(array);
        sizer=TRUE;
        continue;
      }

      if (!strncmp(checks[r],"minw=",5)) {
        array=lives_strsplit(checks[r],"=",2);
        val=atoi(array[1]);
        if (width<val) width=val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r],"minh=",5)) {
        array=lives_strsplit(checks[r],"=",2);
        val=atoi(array[1]);
        if (height<val) height=val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r],"maxh=",5)) {
        array=lives_strsplit(checks[r],"=",2);
        val=atoi(array[1]);
        if (height>val) height=val;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r],"maxw=",5)) {
        array=lives_strsplit(checks[r],"=",2);
        val=atoi(array[1]);
        if (width>val) width=val;
        lives_strfreev(array);
        continue;
      }


      if (!strncmp(checks[r],"asigned=",8)&&
          ((mainw->save_with_sound||rdet!=NULL)&&(resaudw==NULL||
              resaudw->aud_checkbutton==NULL||
              lives_toggle_button_get_active
              (LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton))))&&
          prefs->encoder.audio_codec!=AUDIO_CODEC_NONE
          &&(arate*achans*asampsize)) {
        array=lives_strsplit(checks[r],"=",2);
        if (!strcmp(array[1],"signed")) {
          asigned=1;
        }

        if (!strcmp(array[1],"unsigned")) {
          asigned=2;
        }

        lives_strfreev(array);

        if (asigned!=0&&!capable->has_sox_sox) {
          do_encoder_sox_error();
          lives_strfreev(checks);
          return FALSE;
        }
        continue;
      }


      if (!strncmp(checks[r],"arate=",6)&&((mainw->save_with_sound||rdet!=NULL)&&(resaudw==NULL||
                                           resaudw->aud_checkbutton==NULL||
                                           lives_toggle_button_get_active
                                           (LIVES_TOGGLE_BUTTON
                                            (resaudw->aud_checkbutton))))&&
          prefs->encoder.audio_codec!=AUDIO_CODEC_NONE&&(arate*achans*asampsize)) {
        // we only perform this test if we are encoding with audio
        // find next highest allowed rate from list,
        // if none are higher, use the highest
        int allowed_arate;
        best_arate_delta=1000000000;

        array=lives_strsplit(checks[r],"=",2);
        numtok=get_token_count(array[1],';');
        array2=lives_strsplit(array[1],";",numtok);
        for (i=0; i<numtok; i++) {
          allowed_arate=atoi(array2[i]);
          if (allowed_arate>=arate) {
            if (allowed_arate-arate<best_arate_delta) {
              best_arate_delta=allowed_arate-arate;
              best_arate=allowed_arate;
            }
          } else if (allowed_arate>best_arate) best_arate=allowed_arate;
        }
        lives_strfreev(array2);
        lives_strfreev(array);

        if (!capable->has_sox_sox) {
          do_encoder_sox_error();
          lives_strfreev(checks);
          return FALSE;
        }
        continue;
      }

      if (!strncmp(checks[r],"hblock=",7)) {
        // width must be a multiple of this
        array=lives_strsplit(checks[r],"=",2);
        hblock=atoi(array[1]);
        width=(int)(width/hblock+.5)*hblock;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r],"vblock=",7)) {
        // height must be a multiple of this
        array=lives_strsplit(checks[r],"=",2);
        vblock=atoi(array[1]);
        height=(int)(height/vblock+.5)*vblock;
        lives_strfreev(array);
        continue;
      }

      if (!strncmp(checks[r],"aspect=",7)) {
        // we calculate the nearest smaller frame size using aspect,
        // hblock and vblock
        calc_aspect=TRUE;
        array=lives_strsplit(checks[r],"=",2);
        lives_snprintf(aspect_buffer,512,"%s",array[1]);
        lives_strfreev(array);
        continue;
      }
    }

    /// end restrictions
    lives_strfreev(checks);

    if (!mainw->osc_auto&&calc_aspect&&!sizer) {
      // we calculate this last, after getting hblock and vblock sizes
      char **array3;
      double allowed_aspect;
      int xwidth=width;
      int xheight=height;

      width=height=1000000;

      numtok=get_token_count(aspect_buffer,';');
      array2=lives_strsplit(aspect_buffer,";",numtok);

      // see if we can get a width:height which is nearer an aspect than
      // current width:height

      for (i=0; i<numtok; i++) {
        array3=lives_strsplit(array2[i],":",2);
        allowed_aspect=lives_strtod(array3[0],NULL)/lives_strtod(array3[1],NULL);
        lives_strfreev(array3);
        minimise_aspect_delta(allowed_aspect,hblock,vblock,xwidth,xheight,&width,&height);
      }
      lives_strfreev(array2);

      // allow override if current width and height are integer multiples of blocks
      if (owidth%hblock==0&&oheight%vblock==0) allow_aspect_override=TRUE;

      // end recheck
    }

    // fps can't be altered if we have a multitrack event_list
    if (mainw->multitrack!=NULL&&mainw->multitrack->event_list!=NULL) best_fps_delta=0.;

    if (sizer) allow_aspect_override=FALSE;

  }



  // if we have min or max size, make sure we fit within that


  if (((width!=owidth||height!=oheight)&&width*height>0)||(best_fps_delta>0.)||(best_arate_delta>0&&best_arate>0)||
      best_arate<0||asigned!=0||swap_endian) {
    boolean ofx1_bool=mainw->fx1_bool;
    mainw->fx1_bool=FALSE;
    if ((width!=owidth||height!=oheight)&&width*height>0) {
      if (!capable->has_convert&&rdet==NULL&&mainw->fx_candidates[FX_CANDIDATE_RESIZER].delegate==-1) {
        if (allow_aspect_override) {
          width=owidth;
          height=oheight;
        }
      }
    }
    if (rdet!=NULL&&!rdet->is_encoding) {
      rdet->arate=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_arate)));
      rdet->achans=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_achans)));
      rdet->asamps=(int)atoi(lives_entry_get_text(LIVES_ENTRY(resaudw->entry_asamps)));
      rdet->aendian=get_signed_endian(lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned)),
                                      lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend)));

      if (swap_endian||width!=rdet->width||height!=rdet->height||best_fps_delta!=0.||best_arate!=rdet->arate||
          ((asigned==1&&(rdet->aendian&AFORM_UNSIGNED))||(asigned==2&&!(rdet->aendian&AFORM_SIGNED)))) {

        if (rdet_suggest_values(width,height,best_fps,best_fps_num,best_fps_denom,best_arate,asigned,swap_endian,
                                allow_aspect_override,(best_fps_delta==0.))) {
          char *arate_string;
          rdet->width=width;
          rdet->height=height;
          if (best_arate!=-1) rdet->arate=best_arate;
          else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->aud_checkbutton),FALSE);

          if (asigned==1) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_signed),TRUE);
          else if (asigned==2) lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_unsigned),TRUE);

          if (swap_endian) {
            if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend)))
              lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_bigend),TRUE);
            else lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(resaudw->rb_littleend),TRUE);
          }

          if (best_fps_delta>0.) {
            if (best_fps_denom>0) {
              rdet->fps=(best_fps_num*1.)/(best_fps_denom*1.);
            } else rdet->fps=best_fps;
            lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_fps),rdet->fps);
          }
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_width),rdet->width);
          lives_spin_button_set_value(LIVES_SPIN_BUTTON(rdet->spinbutton_height),rdet->height);
          if (best_arate!=-1) {
            arate_string=lives_strdup_printf("%d",best_arate);
            lives_entry_set_text(LIVES_ENTRY(resaudw->entry_arate),arate_string);
            lives_free(arate_string);
          }
          rdet->suggestion_followed=TRUE;
          return TRUE;
        }
      }
      return FALSE;
    }

    if (mainw->osc_auto||do_encoder_restrict_dialog(width,height,best_fps,best_fps_num,best_fps_denom,best_arate,
        asigned,swap_endian,allow_aspect_override,save_all)) {
      if (!mainw->fx1_bool&&mainw->osc_enc_width==0) {
        width=owidth;
        height=oheight;
      }

      if (!auto_resample_resize(width,height,best_fps,best_fps_num,best_fps_denom,best_arate,asigned,swap_endian)) {
        mainw->fx1_bool=ofx1_bool;
        return FALSE;
      }
    } else {
      mainw->fx1_bool=ofx1_bool;
      return FALSE;
    }
  }
  return TRUE;
}




LiVESList *filter_encoders_by_img_ext(LiVESList *encoders, const char *img_ext) {
  LiVESList *encoder_capabilities=NULL;
  LiVESList *list=encoders,*listnext;
  int caps;

  register int i;

  char *blacklist[]= {
    NULL,
    NULL
  };

  // something broke as of python 2.7.2, and python 3 files now just hang
  if (capable->python_version<3000000) blacklist[0]=lives_strdup("multi_encoder3");

  while (list!=NULL) {
    boolean skip=FALSE;
    i=0;

    listnext=list->next;

    while (blacklist[i]!=NULL) {
      if (strlen((char *)list->data)==strlen(blacklist[i])&&!strcmp((char *)list->data,blacklist[i])) {
        // skip blacklisted encoders
        lives_free((livespointer)list->data);
        encoders=lives_list_delete_link(encoders,list);
        skip=TRUE;
        break;
      }
      i++;
    }
    if (skip) {
      list=listnext;
      continue;
    }

    if (!strcmp(img_ext,LIVES_FILE_EXT_JPG)) {
      list=listnext;
      continue;
    }

    if ((encoder_capabilities=plugin_request(PLUGIN_ENCODERS,(char *)list->data,"get_capabilities"))==NULL) {
      lives_free((livespointer)list->data);
      encoders=lives_list_delete_link(encoders,list);
    } else {
      caps=atoi((char *)lives_list_nth_data(encoder_capabilities,0));
      if (!(caps&CAN_ENCODE_PNG)&&!strcmp(img_ext,LIVES_FILE_EXT_PNG)) {
        lives_free((livespointer)list->data);
        encoders=lives_list_delete_link(encoders,list);
      }

      lives_list_free_strings(encoder_capabilities);
      lives_list_free(encoder_capabilities);

    }

    list=listnext;
  }

  for (i=0; blacklist[i]!=NULL; i++) lives_free(blacklist[i]);

  return encoders;

}



//////////////////////////////////////////////////////
// decoder plugins


LIVES_INLINE boolean decplugin_supports_palette(const lives_decoder_t *dplug, int palette) {
  register int i=0;
  int cpal;
  while ((cpal=dplug->cdata->palettes[i++])!=WEED_PALETTE_END) if (cpal==palette) return TRUE;
  return FALSE;
}



static LiVESList *load_decoders(void) {
  lives_decoder_sys_t *dplug;
  char *decplugdir=lives_strdup_printf("%s%s%s",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS);
  LiVESList *dlist=NULL;
#ifndef IS_MINGW
  LiVESList *decoder_plugins_o=get_plugin_list(PLUGIN_DECODERS,TRUE,decplugdir,"-so");
#else
  LiVESList *decoder_plugins_o=get_plugin_list(PLUGIN_DECODERS,TRUE,decplugdir,"-dll");
#endif
  LiVESList *decoder_plugins=decoder_plugins_o;

  char *blacklist[2]= {
    "zyavformat_decoder",
    NULL
  };

  char *dplugname;
  boolean skip;

  register int i;

  while (decoder_plugins!=NULL) {
    skip=FALSE;
    dplugname=(char *)decoder_plugins->data;
    for (i=0; blacklist[i]!=NULL; i++) {
      if (!strcmp(dplugname,blacklist[i])) {
        // skip blacklisted decoders
        skip=TRUE;
        break;
      }
    }
    if (!skip) {
      dplug=open_decoder_plugin((char *)decoder_plugins->data);
      if (dplug!=NULL) dlist=lives_list_append(dlist,(livespointer)dplug);
    }
    lives_free((livespointer)decoder_plugins->data);
    decoder_plugins=decoder_plugins->next;
  }

  lives_list_free(decoder_plugins_o);

  if (dlist==NULL) {
    char *msg=lives_strdup_printf(_("\n\nNo decoders found in %s !\n"),decplugdir);
    LIVES_WARN(msg);
    d_print(msg);
    lives_free(msg);
  }

  lives_free(decplugdir);
  return dlist;
}



static boolean sanity_check_cdata(lives_clip_data_t *cdata) {
  if (cdata->nframes<=0 || cdata->nframes >= INT_MAX) {
    return FALSE;
  }

  // no usable palettes found
  if (cdata->palettes[0]==WEED_PALETTE_END) return FALSE;

  // all checks passed - OK
  return TRUE;

}


typedef struct {
  LiVESList *disabled;
  lives_decoder_t *dplug;
  lives_clip_t *sfile;
} tdp_data;


lives_decoder_t *clone_decoder(int fileno) {
  lives_decoder_t *dplug;
  const lives_decoder_sys_t *dpsys;
  lives_clip_data_t *cdata;

  if (mainw->files[fileno]==NULL||mainw->files[fileno]->ext_src==NULL) return NULL;

  cdata=((lives_decoder_sys_t *)((lives_decoder_t *)mainw->files[fileno]->ext_src)->decoder)->get_clip_data
        (NULL,((lives_decoder_t *)mainw->files[fileno]->ext_src)->cdata);

  if (cdata==NULL) return NULL;

  dplug=(lives_decoder_t *)lives_malloc(sizeof(lives_decoder_t));

  dpsys=((lives_decoder_t *)mainw->files[fileno]->ext_src)->decoder;

  dplug->decoder=dpsys;
  dplug->cdata=cdata;

  return dplug;
}


static lives_decoder_t *try_decoder_plugins(char *file_name, LiVESList *disabled, const lives_clip_data_t *fake_cdata) {
  lives_decoder_t *dplug=(lives_decoder_t *)lives_malloc(sizeof(lives_decoder_t));
  LiVESList *decoder_plugin=mainw->decoder_list;

  while (decoder_plugin!=NULL) {
    lives_decoder_sys_t *dpsys=(lives_decoder_sys_t *)decoder_plugin->data;

    if (lives_list_strcmp_index(disabled,dpsys->name)!=-1) {
      // check if (user) disabled this decoder
      decoder_plugin=decoder_plugin->next;
      continue;
    }

    //#define DEBUG_DECPLUG
#ifdef DEBUG_DECPLUG
    g_print("trying decoder %s\n",dpsys->name);
#endif

    dplug->cdata=(dpsys->get_clip_data)(file_name,fake_cdata);

    if (dplug->cdata!=NULL) {
      // check for sanity

      if (!sanity_check_cdata(dplug->cdata)) {
        decoder_plugin=decoder_plugin->next;
        continue;
      }

      //////////////////////

      dplug->decoder=dpsys;

      if (strncmp(dpsys->name,"zz",2)) {
        mainw->decoder_list=lives_list_move_to_first(mainw->decoder_list, decoder_plugin);
      }
      break;
    }
    decoder_plugin=decoder_plugin->next;
  }
  if (decoder_plugin==NULL) {
    lives_free(dplug);
    dplug=NULL;
  }

  return dplug;
}





const lives_clip_data_t *get_decoder_cdata(int fileno, LiVESList *disabled, const lives_clip_data_t *fake_cdata) {
  // pass file to each decoder (demuxer) plugin in turn, until we find one that can parse
  // the file
  // NULL is returned if no decoder plugin recognises the file - then we
  // fall back to other methods

  // otherwise we return data for the clip as supplied by the decoder plugin

  // If the file does not exist, we set mainw->error=TRUE and return NULL

  // If we find a plugin we also set sfile->ext_src to point to a newly created decoder_plugin_t

  lives_decoder_t *dplug;

  LiVESList *dlist=NULL,*xdisabled;

  lives_clip_t *sfile=mainw->files[fileno];

  char decplugname[PATH_MAX];

  mainw->error=FALSE;

  if (!lives_file_test(sfile->file_name, LIVES_FILE_TEST_EXISTS)) {
    mainw->error=TRUE;
    return NULL;
  }

  memset(decplugname,0,1);

  // check sfile->file_name against each decoder plugin,
  // until we get non-NULL cdata

  sfile->ext_src=NULL;

  lives_set_cursor_style(LIVES_CURSOR_BUSY,NULL);

  if (!mainw->decoders_loaded) {
    mainw->decoder_list=load_decoders();
    mainw->decoders_loaded=TRUE;
  }

  xdisabled=lives_list_copy(disabled);

  if (fake_cdata!=NULL) {
    get_clip_value(fileno,CLIP_DETAILS_DECODER_NAME,decplugname,PATH_MAX);

    if (strlen(decplugname)) {
      LiVESList *decoder_plugin=mainw->decoder_list;
      if (!strncmp(decplugname,"zz",2)) {
        dlist=lives_list_copy(mainw->decoder_list);
      }

      while (decoder_plugin!=NULL) {
        lives_decoder_sys_t *dpsys=(lives_decoder_sys_t *)decoder_plugin->data;
        if (!strcmp(dpsys->name,decplugname)) {
          mainw->decoder_list=lives_list_move_to_first(mainw->decoder_list, decoder_plugin);
          break;
        }
        decoder_plugin=decoder_plugin->next;
      }

      xdisabled=lives_list_remove(disabled,decplugname);
    }
  }

  dplug=try_decoder_plugins(fake_cdata==NULL?sfile->file_name:NULL,xdisabled,fake_cdata);

  if (strlen(decplugname)) {
    if (!strncmp(decplugname,"zz",2)) {
      if (mainw->decoder_list!=NULL) lives_list_free(mainw->decoder_list);
      mainw->decoder_list=dlist;
    }
  }

  if (xdisabled!=NULL) lives_list_free(xdisabled);

  lives_set_cursor_style(LIVES_CURSOR_NORMAL,NULL);

  if (dplug!=NULL) {
    d_print(_(" using %s"),dplug->decoder->version());
    sfile->ext_src=dplug;
    return dplug->cdata;
  }

  if (dplug!=NULL) return dplug->cdata;
  return NULL;
}






// close one instance of dplug
void close_decoder_plugin(lives_decoder_t *dplug) {
  lives_clip_data_t *cdata;

  if (dplug==NULL) return;

  cdata=dplug->cdata;

  if (cdata!=NULL)(*dplug->decoder->clip_data_free)(cdata);

  lives_free(dplug);

}


static void unload_decoder_plugin(lives_decoder_sys_t *dplug) {
  if (dplug->module_unload!=NULL)(*dplug->module_unload)();

  if (dplug->name!=NULL) {
    lives_free(dplug->name);
  }

  dlclose(dplug->handle);
  lives_free(dplug);
}


void unload_decoder_plugins(void) {
  LiVESList *dplugs=mainw->decoder_list;

  while (dplugs!=NULL) {
    unload_decoder_plugin((lives_decoder_sys_t *)dplugs->data);
    dplugs=dplugs->next;
  }

  lives_list_free(mainw->decoder_list);
  mainw->decoder_list=NULL;
  mainw->decoders_loaded=FALSE;
}



lives_decoder_sys_t *open_decoder_plugin(const char *plname) {
  lives_decoder_sys_t *dplug;

  char *plugname;
  boolean OK=TRUE;
  const char *err;

  if (!strcmp(plname,"ogg_theora_decoder")) {
    // no longer compatible
    return NULL;
  }

  dplug=(lives_decoder_sys_t *)lives_malloc(sizeof(lives_decoder_sys_t));

  dplug->name=NULL;

#ifndef IS_MINGW
  plugname=lives_strdup_printf("%s%s%s/%s.so",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS,plname);
#else
  plugname=lives_strdup_printf("%s%s%s/%s.dll",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS,plname);
#endif

  dplug->handle=dlopen(plugname,RTLD_LAZY);
  lives_free(plugname);

  if (dplug->handle==NULL) {
    d_print(_("\n\nFailed to open decoder plugin %s\nError was %s\n"),plname,dlerror());
    lives_free(dplug);
    return NULL;
  }

  if ((dplug->version=(const char* ( *)())dlsym(dplug->handle,"version"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->get_clip_data=(lives_clip_data_t* ( *)(char *, const lives_clip_data_t *))
                            dlsym(dplug->handle,"get_clip_data"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->get_frame=(boolean( *)(const lives_clip_data_t *, int64_t, int *, int, void **))
                        dlsym(dplug->handle,"get_frame"))==NULL) {
    OK=FALSE;
  }
  if ((dplug->clip_data_free=(void ( *)(lives_clip_data_t *))dlsym(dplug->handle,"clip_data_free"))==NULL) {
    OK=FALSE;
  }

  if (!OK) {
    d_print(_("\n\nDecoder plugin %s\nis missing a mandatory function.\nUnable to use it.\n"),plname);
    unload_decoder_plugin(dplug);
    lives_free(dplug);
    return NULL;
  }

  // optional
  dplug->module_check_init=(const char* ( *)())dlsym(dplug->handle,"module_check_init");
  dplug->set_palette=(boolean( *)(lives_clip_data_t *))dlsym(dplug->handle,"set_palette");
  dplug->module_unload=(void ( *)())dlsym(dplug->handle,"module_unload");
  dplug->rip_audio=(int64_t ( *)(const lives_clip_data_t *, const char *, int64_t, int64_t, unsigned char **))
                   dlsym(dplug->handle,"rip_audio");
  dplug->rip_audio_cleanup=(void ( *)(const lives_clip_data_t *))dlsym(dplug->handle,"rip_audio_cleanup");

  if (dplug->module_check_init!=NULL) {
    err=(*dplug->module_check_init)();

    if (err!=NULL) {
      lives_snprintf(mainw->msg,512,"%s",err);
      unload_decoder_plugin(dplug);
      lives_free(dplug);
      return NULL;
    }
  }

  dplug->name=lives_strdup(plname);
  return dplug;
}




void get_mime_type(char *text, int maxlen, const lives_clip_data_t *cdata) {
  char *audname;

  if (cdata->container_name==NULL||!strlen(cdata->container_name)) lives_snprintf(text,40,"%s",_("unknown"));
  else lives_snprintf(text,40,"%s",cdata->container_name);

  if ((cdata->video_name==NULL||!strlen(cdata->video_name))&&(cdata->audio_name==NULL||!strlen(cdata->audio_name))) return;

  if (cdata->video_name==NULL) lives_strappend(text,40,_("/unknown"));
  else {
    char *vidname=lives_strdup_printf("/%s",cdata->video_name);
    lives_strappend(text,40,vidname);
    lives_free(vidname);
  }

  if (cdata->audio_name==NULL||!strlen(cdata->audio_name)) {
    if (cfile->achans==0) return;
    audname=lives_strdup_printf("/%s",_("unknown"));
  } else
    audname=lives_strdup_printf("/%s",cdata->audio_name);
  lives_strappend(text,40,audname);
  lives_free(audname);
}



static void dpa_ok_clicked(LiVESButton *button, livespointer user_data) {
  lives_general_button_clicked(button,NULL);

  if (prefsw!=NULL) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
    if (string_lists_differ(future_prefs->disabled_decoders,future_prefs->disabled_decoders_new))
      apply_button_set_enabled(NULL,NULL);
  }

  if (future_prefs->disabled_decoders!=NULL) {
    lives_list_free_strings(future_prefs->disabled_decoders);
    lives_list_free(future_prefs->disabled_decoders);
  }

  future_prefs->disabled_decoders=future_prefs->disabled_decoders_new;

}


static void dpa_cancel_clicked(LiVESButton *button, livespointer user_data) {
  lives_general_button_clicked(button,NULL);

  if (prefsw!=NULL) {
    lives_window_present(LIVES_WINDOW(prefsw->prefs_dialog));
    lives_xwindow_raise(lives_widget_get_xwindow(prefsw->prefs_dialog));
  }

  if (future_prefs->disabled_decoders_new!=NULL) {
    lives_list_free_strings(future_prefs->disabled_decoders_new);
    lives_list_free(future_prefs->disabled_decoders_new);
  }


}

static void on_dpa_cb_toggled(LiVESToggleButton *button, char *decname) {
  if (!lives_toggle_button_get_active(button))
    // unchecked is disabled
    future_prefs->disabled_decoders_new=lives_list_append(future_prefs->disabled_decoders_new,lives_strdup(decname));
  else
    future_prefs->disabled_decoders_new=lives_list_delete_string(future_prefs->disabled_decoders_new,decname);
}


void on_decplug_advanced_clicked(LiVESButton *button, livespointer user_data) {
  LiVESList *decoder_plugin;

  LiVESWidget *hbox;
  LiVESWidget *vbox;
  LiVESWidget *checkbutton;
  LiVESWidget *scrolledwindow;
  LiVESWidget *label;
  LiVESWidget *dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *cancelbutton;
  LiVESWidget *okbutton;

  char *ltext;
  char *decplugdir=lives_strdup_printf("%s%s%s",prefs->lib_dir,PLUGIN_EXEC_DIR,PLUGIN_DECODERS);

  if (!mainw->decoders_loaded) {
    mainw->decoder_list=load_decoders();
    mainw->decoders_loaded=TRUE;
  }

  decoder_plugin=mainw->decoder_list;

  dialog = lives_standard_dialog_new(_("LiVES: - Decoder Plugins"),FALSE,DEF_DIALOG_WIDTH,DEF_DIALOG_HEIGHT);

  if (prefs->show_gui) {
    if (prefsw!=NULL) lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(prefsw->prefs_dialog));
    else {
      if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(mainw->LiVES));
      else lives_window_set_transient_for(LIVES_WINDOW(dialog),LIVES_WINDOW(mainw->multitrack->window));
    }
  }

  dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(dialog));

  vbox = lives_vbox_new(FALSE, 0);

  scrolledwindow = lives_standard_scrolled_window_new(RFX_WINSIZE_H, RFX_WINSIZE_V, vbox);

  lives_container_add(LIVES_CONTAINER(dialog_vbox), scrolledwindow);

  label=lives_standard_label_new(_("Enabled Video Decoders (uncheck to disable)"));
  lives_box_pack_start(LIVES_BOX(vbox), label, FALSE, FALSE, widget_opts.packing_height);

  while (decoder_plugin!=NULL) {
    lives_decoder_sys_t *dpsys=(lives_decoder_sys_t *)decoder_plugin->data;
    hbox = lives_hbox_new(FALSE, 0);
    lives_box_pack_start(LIVES_BOX(vbox), hbox, FALSE, FALSE, widget_opts.packing_height);
    ltext=lives_strdup_printf("%s   (%s)",dpsys->name,(*dpsys->version)());

    checkbutton=lives_standard_check_button_new(ltext,FALSE,LIVES_BOX(hbox),NULL);

    lives_free(ltext);

    lives_toggle_button_set_active(LIVES_TOGGLE_BUTTON(checkbutton),
                                   lives_list_strcmp_index(future_prefs->disabled_decoders,dpsys->name)==-1);

    lives_signal_connect_after(LIVES_GUI_OBJECT(checkbutton), LIVES_WIDGET_TOGGLED_SIGNAL,
                               LIVES_GUI_CALLBACK(on_dpa_cb_toggled),
                               dpsys->name);

    decoder_plugin=decoder_plugin->next;
  }


  cancelbutton = lives_button_new_from_stock(LIVES_STOCK_CANCEL,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), cancelbutton, LIVES_RESPONSE_CANCEL);

  okbutton = lives_button_new_from_stock(LIVES_STOCK_OK,NULL);
  lives_dialog_add_action_widget(LIVES_DIALOG(dialog), okbutton, LIVES_RESPONSE_OK);
  lives_widget_set_can_focus_and_default(okbutton);

  lives_signal_connect(LIVES_GUI_OBJECT(cancelbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(dpa_cancel_clicked),
                       NULL);

  lives_signal_connect(LIVES_GUI_OBJECT(okbutton), LIVES_WIDGET_CLICKED_SIGNAL,
                       LIVES_GUI_CALLBACK(dpa_ok_clicked),
                       NULL);

  lives_widget_show_all(dialog);
  lives_window_present(LIVES_WINDOW(dialog));
  lives_xwindow_raise(lives_widget_get_xwindow(dialog));

  future_prefs->disabled_decoders_new=lives_list_copy_strings(future_prefs->disabled_decoders);

  lives_free(decplugdir);
}

///////////////////////////////////////////////////////
// rfx plugin functions



boolean check_rfx_for_lives(lives_rfx_t *rfx) {
  // check that an RFX is suitable for loading (cf. check_for_lives in effects-weed.c)
  if (rfx->num_in_channels==2&&rfx->props&RFX_PROPS_MAY_RESIZE) {
    d_print(_("Failed to load %s, transitions may not resize.\n"),rfx->name);
    return FALSE;
  }
  return TRUE;
}

void do_rfx_cleanup(lives_rfx_t *rfx) {
  char *com;
  char *dir=NULL;

  if (rfx==&mainw->rendered_fx[0]) return;

  switch (rfx->status) {
  case RFX_STATUS_BUILTIN:
    dir=lives_build_filename(prefs->lib_dir,PLUGIN_EXEC_DIR,NULL);
    com=lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"",prefs->backend_sync,
                            cfile->handle,cfile->start,cfile->end,dir,
                            PLUGIN_RENDERED_EFFECTS_BUILTIN,rfx->name);
    break;
  case RFX_STATUS_CUSTOM:
    dir=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,NULL);
    com=lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"",prefs->backend_sync,
                            cfile->handle,cfile->start,cfile->end,dir,
                            PLUGIN_RENDERED_EFFECTS_CUSTOM,rfx->name);
    break;
  case RFX_STATUS_TEST:
    dir=lives_build_filename(capable->home_dir,LIVES_CONFIG_DIR,NULL);
    com=lives_strdup_printf("%s plugin_clear \"%s\" %d %d \"%s\" \"%s\" \"%s\"",prefs->backend_sync,
                            cfile->handle,cfile->start,cfile->end,dir,
                            PLUGIN_RENDERED_EFFECTS_TEST,rfx->name);
    break;
  default:
    return;
  }

  if (dir!=NULL) lives_free(dir);

  // if the command fails we just give a warning
  lives_system(com,FALSE);

  lives_free(com);
}




void render_fx_get_params(lives_rfx_t *rfx, const char *plugin_name, short status) {
  // create lives_param_t array from plugin supplied values
  LiVESList *parameter_list;
  int param_idx,i;
  lives_param_t *cparam;
  char **param_array;
  char *line;
  int len;

  switch (status) {
  case RFX_STATUS_BUILTIN:
    parameter_list=plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_BUILTIN,plugin_name,"get_parameters");
    break;
  case RFX_STATUS_CUSTOM:
    parameter_list=plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_CUSTOM,plugin_name,"get_parameters");
    break;
  case RFX_STATUS_SCRAP:
    parameter_list=plugin_request_by_line(PLUGIN_RFX_SCRAP,plugin_name,"get_parameters");
    break;
  default:
    parameter_list=plugin_request_by_line(PLUGIN_RENDERED_EFFECTS_TEST,plugin_name,"get_parameters");
    break;
  }

  if (parameter_list==NULL) {
    rfx->num_params=0;
    rfx->params=NULL;
    return;
  }

  threaded_dialog_spin();
  rfx->num_params=lives_list_length(parameter_list);
  rfx->params=(lives_param_t *)lives_malloc(rfx->num_params*sizeof(lives_param_t));

  for (param_idx=0; param_idx<rfx->num_params; param_idx++) {

    line=(char *)lives_list_nth_data(parameter_list,param_idx);

    len=get_token_count(line,(unsigned int)rfx->delim[0]);

    if (len<3) continue;

    param_array=lives_strsplit(line,rfx->delim,-1);

    cparam=&rfx->params[param_idx];
    cparam->name=lives_strdup(param_array[0]);
    cparam->label=lives_strdup(param_array[1]);
    cparam->desc=NULL;
    cparam->use_mnemonic=TRUE;
    cparam->interp_func=NULL;
    cparam->display_func=NULL;
    cparam->hidden=0;
    cparam->wrap=FALSE;
    cparam->transition=FALSE;
    cparam->step_size=1.;
    cparam->group=0;
    cparam->max=0.;
    cparam->reinit=FALSE;
    cparam->changed=FALSE;
    cparam->change_blocked=FALSE;
    cparam->source=NULL;
    cparam->source_type=LIVES_RFX_SOURCE_RFX;
    cparam->special_type=LIVES_PARAM_SPECIAL_TYPE_NONE;
    cparam->special_type_index=0;

#ifdef DEBUG_RENDER_FX_P
    lives_printerr("Got parameter %s\n",cparam->name);
#endif
    cparam->dp=0;
    cparam->list=NULL;

    if (!strncmp(param_array[2],"num",3)) {
      cparam->dp=atoi(param_array[2]+3);
      cparam->type=LIVES_PARAM_NUM;
    } else if (!strncmp(param_array[2],"bool",4)) {
      cparam->type=LIVES_PARAM_BOOL;
    } else if (!strncmp(param_array[2],"colRGB24",8)) {
      cparam->type=LIVES_PARAM_COLRGB24;
    } else if (!strncmp(param_array[2],"string",8)) {
      cparam->type=LIVES_PARAM_STRING;
    } else if (!strncmp(param_array[2],"string_list",8)) {
      cparam->type=LIVES_PARAM_STRING_LIST;
    } else continue;

    if (cparam->dp) {
      double val;
      if (len<6) continue;
      val=lives_strtod(param_array[3],NULL);
      cparam->value=lives_malloc(sizdbl);
      cparam->def=lives_malloc(sizdbl);
      set_double_param(cparam->def,val);
      set_double_param(cparam->value,val);
      cparam->min=lives_strtod(param_array[4],NULL);
      cparam->max=lives_strtod(param_array[5],NULL);
      if (len>6) {
        cparam->step_size=lives_strtod(param_array[6],NULL);
        if (cparam->step_size==0.) cparam->step_size=1./(double)lives_10pow(cparam->dp);
        else if (cparam->step_size<0.) {
          cparam->step_size=-cparam->step_size;
          cparam->wrap=TRUE;
        }
      }
    } else if (cparam->type==LIVES_PARAM_COLRGB24) {
      short red;
      short green;
      short blue;
      if (len<6) continue;
      red=(short)atoi(param_array[3]);
      green=(short)atoi(param_array[4]);
      blue=(short)atoi(param_array[5]);
      cparam->value=lives_malloc(sizeof(lives_colRGB24_t));
      cparam->def=lives_malloc(sizeof(lives_colRGB24_t));
      set_colRGB24_param(cparam->def,red,green,blue);
      set_colRGB24_param(cparam->value,red,green,blue);
    } else if (cparam->type==LIVES_PARAM_STRING) {
      if (len<4) continue;
      cparam->value=lives_strdup(_(param_array[3]));
      cparam->def=lives_strdup(_(param_array[3]));
      if (len>4) cparam->max=(double)atoi(param_array[4]);
      if (cparam->max==0.||cparam->max>RFX_MAXSTRINGLEN) cparam->max=RFX_MAXSTRINGLEN;
    } else if (cparam->type==LIVES_PARAM_STRING_LIST) {
      if (len<4) continue;
      cparam->value=lives_malloc(sizint);
      cparam->def=lives_malloc(sizint);
      *(int *)cparam->def=atoi(param_array[3]);
      if (len>4) {
        cparam->list=array_to_string_list(param_array,3,len);
      } else {
        set_int_param(cparam->def,0);
      }
      set_int_param(cparam->value,get_int_param(cparam->def));
    } else {
      // int or bool
      int val;
      if (len<4) continue;
      val=atoi(param_array[3]);
      cparam->value=lives_malloc(sizint);
      cparam->def=lives_malloc(sizint);
      set_int_param(cparam->def,val);
      set_int_param(cparam->value,val);
      if (cparam->type==LIVES_PARAM_BOOL) {
        cparam->min=0;
        cparam->max=1;
        if (len>4) cparam->group=atoi(param_array[4]);
      } else {
        if (len<6) continue;
        cparam->min=(double)atoi(param_array[4]);
        cparam->max=(double)atoi(param_array[5]);
        if (len>6) {
          cparam->step_size=(double)atoi(param_array[6]);
          if (cparam->step_size==0.) cparam->step_size=1.;
          else if (cparam->step_size<0.) {
            cparam->step_size=-cparam->step_size;
            cparam->wrap=TRUE;
          }
        }
      }
    }

    for (i=0; i<MAX_PARAM_WIDGETS; i++) {
      cparam->widgets[i]=NULL;
    }
    cparam->onchange=FALSE;
    lives_strfreev(param_array);
  }
  lives_list_free_strings(parameter_list);
  lives_list_free(parameter_list);
  threaded_dialog_spin();
}


LiVESList *array_to_string_list(char **array, int offset, int len) {
  // build a LiVESList from an array.
  int i;

  char *string,*tmp;
  LiVESList *slist=NULL;

  for (i=offset+1; i<len; i++) {
    string=subst((tmp=L2U8(array[i])),"\\n","\n");
    lives_free(tmp);

    // omit a last empty string
    if (i<len-1||strlen(string)) {
      slist=lives_list_append(slist, string);
    } else lives_free(string);
  }

  return slist;
}



void sort_rfx_array(lives_rfx_t *in, int num) {
  // sort rfx array into UTF-8 order by menu entry
#ifdef GUI_QT
  QString min_string, str;
#endif

  lives_rfx_t *rfx;

#ifdef GUI_GTK
  char *min_string=NULL;
#endif
  char *tmp=NULL;

  boolean used[num];

  int start=1,min_val=0;
  int sorted=1;

  register int i;

  for (i=0; i<num; i++) {
    used[i]=FALSE;
  }

  rfx=mainw->rendered_fx=(lives_rfx_t *)lives_malloc((num+1)*sizeof(lives_rfx_t));

  rfx->name=lives_strdup(in[0].name);
  rfx->menu_text=lives_strdup(in[0].menu_text);
  rfx->action_desc=lives_strdup(in[0].action_desc);
  rfx->props=in[0].props;
  rfx->num_params=0;
  rfx->min_frames=1;
  rfx->params=NULL;
  rfx->source=NULL;
  rfx->source_type=LIVES_RFX_SOURCE_RFX;
  rfx->is_template=FALSE;
  rfx->extra=NULL;

  while (sorted<=num) {
    for (i=start; i<=num; i++) {
      if (!used[i-1]) {
        if (min_string==NULL) {
#ifdef GUI_GTK
          min_string=g_utf8_collate_key(in[i].menu_text,-1);
#endif
#ifdef GUI_QT
          min_string = QString::fromUtf8(in[i].menu_text);
#endif
          min_val=i;
        } else {
#ifdef GUI_GTK
          if (strcmp(min_string,(tmp=g_utf8_collate_key(in[i].menu_text,-1)))==1) {
            lives_free(min_string);
            min_string=g_utf8_collate_key(in[i].menu_text,-1);
#endif
#ifdef GUI_QT
            str = QString::fromUtf8(in[i].menu_text);
            if (str.compare(min_string) < 0) {
              min_string = str;
#endif
              min_val=i;
            }
            if (tmp!=NULL) lives_free(tmp);
          }
        }
      }
      rfx_copy(&in[min_val],&mainw->rendered_fx[sorted++],FALSE);
      used[min_val-1]=TRUE;
#ifdef GUI_GTK
      if (min_string!=NULL) lives_free(min_string);
      min_string=NULL;
#endif
    }

    for (i=0; i<=num; i++) {
      rfx_free(&in[i]);
    }
  }


  void rfx_copy(lives_rfx_t *src, lives_rfx_t *dest, boolean full) {
    // Warning, does not copy all fields (full will do that)
    dest->name=lives_strdup(src->name);
    dest->menu_text=lives_strdup(src->menu_text);
    dest->action_desc=lives_strdup(src->action_desc);
    dest->min_frames=src->min_frames;
    dest->num_in_channels=src->num_in_channels;
    dest->status=src->status;
    dest->props=src->props;
    dest->source_type=src->source_type;
    dest->source=src->source;
    dest->is_template=src->is_template;
    lives_memcpy(dest->delim,src->delim,2);
    if (!full) return;
  }


  void rfx_params_free(lives_rfx_t *rfx) {
    register int i;
    for (i=0; i<rfx->num_params; i++) {
      if (rfx->params[i].type==LIVES_PARAM_UNDISPLAYABLE) continue;
      lives_free(rfx->params[i].name);
      if (rfx->params[i].def!=NULL) lives_free(rfx->params[i].def);
      if (rfx->params[i].value!=NULL) lives_free(rfx->params[i].value);
      if (rfx->params[i].label!=NULL) lives_free(rfx->params[i].label);
      if (rfx->params[i].desc!=NULL) lives_free(rfx->params[i].desc);
      if (rfx->params[i].list!=NULL) {
        lives_list_free_strings(rfx->params[i].list);
        lives_list_free(rfx->params[i].list);
      }
    }
  }



  void rfx_free(lives_rfx_t *rfx) {
    if (mainw->vrfx_update==rfx) mainw->vrfx_update=NULL;

    if (rfx->name!=NULL) lives_free(rfx->name);
    if (rfx->menu_text!=NULL) lives_free(rfx->menu_text);
    if (rfx->action_desc!=NULL) lives_free(rfx->action_desc);

    if (rfx->params!=NULL) {
      rfx_params_free(rfx);
      lives_free(rfx->params);
    }
    if (rfx->extra!=NULL) {
      free(rfx->extra);
    }
    if (rfx->source_type==LIVES_RFX_SOURCE_WEED&&rfx->source!=NULL) {
      weed_instance_unref((weed_plant_t *)rfx->source);
    }
  }


  void rfx_free_all(void) {
    register int i;
    for (i=0; i<=mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+mainw->num_rendered_effects_test; i++) {
      rfx_free(&mainw->rendered_fx[i]);
    }
    lives_free(mainw->rendered_fx);
    mainw->rendered_fx=NULL;
  }


  void param_copy(lives_param_t *src, lives_param_t *dest, boolean full) {
    // rfxbuilder.c uses this to copy params to a temporary copy and back again

    dest->name=lives_strdup(src->name);
    dest->label=lives_strdup(src->label);
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
        dest->def=lives_malloc(sizint);
        lives_memcpy(dest->def,src->def,sizint);
      } else {
        dest->def=lives_malloc(sizdbl);
        lives_memcpy(dest->def,src->def,sizdbl);
      }
      break;
    case LIVES_PARAM_COLRGB24:
      dest->def=lives_malloc(sizeof(lives_colRGB24_t));
      lives_memcpy(dest->def,src->def,sizeof(lives_colRGB24_t));
      break;
    case LIVES_PARAM_STRING:
      dest->def=lives_strdup((char *)src->def);
      break;
    case LIVES_PARAM_STRING_LIST:
      dest->def=lives_malloc(sizint);
      set_int_param(dest->def,get_int_param(src->def));
      if (src->list!=NULL) dest->list=lives_list_copy(src->list);
      break;
    default:
      break;
    }
    if (!full) return;
    // TODO - copy value, copy widgets

  }

  boolean get_bool_param(void *value) {
    boolean ret;
    lives_memcpy(&ret,value,4);
    return ret;
  }

  int get_int_param(void *value) {
    int ret;
    lives_memcpy(&ret,value,sizint);
    return ret;
  }

  double get_double_param(void *value) {
    double ret;
    lives_memcpy(&ret,value,sizdbl);
    return ret;
  }

  void get_colRGB24_param(void *value, lives_colRGB24_t *rgb) {
    lives_memcpy(rgb,value,sizeof(lives_colRGB24_t));
  }

  void get_colRGBA32_param(void *value, lives_colRGBA32_t *rgba) {
    lives_memcpy(rgba,value,sizeof(lives_colRGBA32_t));
  }

  void set_bool_param(void *value, boolean _const) {
    set_int_param(value,!!_const);
  }

  void set_int_param(void *value, int _const) {
    lives_memcpy(value,&_const,sizint);
  }
  void set_double_param(void *value, double _const) {
    lives_memcpy(value,&_const,sizdbl);

  }

  void set_colRGB24_param(void *value, short red, short green, short blue) {
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

  void set_colRGBA32_param(void *value, short red, short green, short blue, short alpha) {
    lives_colRGBA32_t *rgbap=(lives_colRGBA32_t *)value;
    rgbap->red=red;
    rgbap->green=green;
    rgbap->blue=blue;
    rgbap->alpha=alpha;
  }




  ///////////////////////////////////////////////////////////////



  int find_rfx_plugin_by_name(const char *name, short status) {
    int i;
    for (i=1; i<mainw->num_rendered_effects_builtin+mainw->num_rendered_effects_custom+
         mainw->num_rendered_effects_test; i++) {
      if (mainw->rendered_fx[i].name!=NULL&&!strcmp(mainw->rendered_fx[i].name,name)
          &&mainw->rendered_fx[i].status==status)
        return (int)i;
    }
    return -1;
  }



  lives_param_t *weed_params_to_rfx(int npar, weed_plant_t *inst, boolean show_reinits) {
    int i,j;
    lives_param_t *rpar=(lives_param_t *)lives_malloc(npar*sizeof(lives_param_t));
    int param_hint;
    char **list;
    LiVESList *gtk_list=NULL;
    char *string;
    int error;
    int vali;
    double vald;
    weed_plant_t *gui=NULL;
    int listlen;
    int cspace,*cols=NULL,red_min=0,red_max=255,green_min=0,green_max=255,blue_min=0,blue_max=255,*maxi=NULL,*mini=NULL;
    double *colsd;
    double red_mind=0.,red_maxd=1.,green_mind=0.,green_maxd=1.,blue_mind=0.,blue_maxd=1.,*maxd=NULL,*mind=NULL;
    int flags=0;
    int nwpars=0,poffset=0;
    boolean col_int;

    weed_plant_t *wtmpl;
    weed_plant_t **wpars=NULL,*wpar=NULL;

    weed_plant_t *chann,*ctmpl;

    if (weed_plant_has_leaf(inst,"in_parameters")) nwpars=weed_leaf_num_elements(inst,"in_parameters");
    if (nwpars>0) wpars=weed_get_plantptr_array(inst,"in_parameters",&error);

    for (i=0; i<npar; i++) {
      if (i-poffset>=nwpars) {
        // handling for compound fx
        poffset+=nwpars;
        if (wpars!=NULL) lives_free(wpars);
        inst=weed_get_plantptr_value(inst,"host_next_instance",&error);
        if (weed_plant_has_leaf(inst,"in_parameters")) nwpars=weed_leaf_num_elements(inst,"in_parameters");
        else nwpars=0;
        if (nwpars>0) wpars=weed_get_plantptr_array(inst,"in_parameters",&error);
        else wpars=NULL;
        i--;
        continue;
      }

      wpar=wpars[i-poffset];
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
      rpar[i].transition=FALSE;
      rpar[i].wrap=FALSE;
      rpar[i].reinit=FALSE;
      rpar[i].change_blocked=FALSE;
      rpar[i].source=wtmpl;
      rpar[i].source_type=LIVES_RFX_SOURCE_WEED;
      rpar[i].special_type=LIVES_PARAM_SPECIAL_TYPE_NONE;
      rpar[i].special_type_index=0;

      if (flags&WEED_PARAMETER_VARIABLE_ELEMENTS&&!(flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL)) {
        rpar[i].hidden|=HIDDEN_MULTI;
        rpar[i].multi=PVAL_MULTI_ANY;
      } else if (flags&WEED_PARAMETER_ELEMENT_PER_CHANNEL) {
        rpar[i].hidden|=HIDDEN_MULTI;
        rpar[i].multi=PVAL_MULTI_PER_CHANNEL;
      } else rpar[i].multi=PVAL_MULTI_NONE;

      chann=get_enabled_channel(inst,0,TRUE);
      ctmpl=weed_get_plantptr_value(chann,"template",&error);

      if (weed_plant_has_leaf(ctmpl,"is_audio")&&weed_get_boolean_value(ctmpl,"is_audio",&error)==WEED_TRUE) {
        // dont hide multivalued params for audio effects
        rpar[i].hidden=0;
      }

      rpar[i].dp=0;
      rpar[i].min=0.;
      rpar[i].max=0.;
      rpar[i].list=NULL;

      if (flags&WEED_PARAMETER_REINIT_ON_VALUE_CHANGE) {
        rpar[i].reinit=TRUE;
        if (!show_reinits) rpar[i].hidden|=HIDDEN_NEEDS_REINIT;
      } else rpar[i].reinit=FALSE;

      // hide internally connected params for compound fx
      if (weed_plant_has_leaf(wpar,"host_internal_connection")) rpar[i].hidden|=HIDDEN_COMPOUND_INTERNAL;

      ///////////////////////////////
      param_hint=weed_get_int_value(wtmpl,"hint",&error);

      switch (param_hint) {
      case WEED_HINT_SWITCH:
        if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
          rpar[i].hidden|=HIDDEN_MULTI;
        }
        rpar[i].type=LIVES_PARAM_BOOL;
        rpar[i].value=lives_malloc(sizint);
        rpar[i].def=lives_malloc(sizint);
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
        rpar[i].value=lives_malloc(sizint);
        rpar[i].def=lives_malloc(sizint);
        if (weed_plant_has_leaf(wtmpl,"host_default")) {
          vali=weed_get_int_value(wtmpl,"host_default",&error);
        } else if (weed_leaf_num_elements(wtmpl,"default")>0) vali=weed_get_int_value(wtmpl,"default",&error);
        else vali=weed_get_int_value(wtmpl,"new_default",&error);
        set_int_param(rpar[i].def,vali);
        vali=weed_get_int_value(wpar,"value",&error);
        set_int_param(rpar[i].value,vali);
        rpar[i].min=(double)weed_get_int_value(wtmpl,"min",&error);
        rpar[i].max=(double)weed_get_int_value(wtmpl,"max",&error);
        if (weed_plant_has_leaf(wtmpl,"wrap")&&weed_get_boolean_value(wtmpl,"wrap",&error)==WEED_TRUE) rpar[i].wrap=TRUE;
        if (gui!=NULL) {
          if (weed_plant_has_leaf(gui,"choices")) {
            listlen=weed_leaf_num_elements(gui,"choices");
            list=weed_get_string_array(gui,"choices",&error);
            for (j=0; j<listlen; j++) {
              gtk_list=lives_list_append(gtk_list,lives_strdup(list[j]));
              lives_free(list[j]);
            }
            lives_free(list);
            rpar[i].list=lives_list_copy(gtk_list);
            lives_list_free(gtk_list);
            gtk_list=NULL;
            rpar[i].type=LIVES_PARAM_STRING_LIST;
            rpar[i].max=listlen;
          } else if (weed_plant_has_leaf(gui,"step_size"))
            rpar[i].step_size=(double)weed_get_int_value(gui,"step_size",&error);
          if (rpar[i].step_size==0.) rpar[i].step_size=1.;
        }
        break;
      case WEED_HINT_FLOAT:
        if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
          rpar[i].hidden|=HIDDEN_MULTI;
        }
        rpar[i].type=LIVES_PARAM_NUM;
        rpar[i].value=lives_malloc(sizdbl);
        rpar[i].def=lives_malloc(sizdbl);
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
        rpar[i].dp=2;
        if (gui!=NULL) {
          if (weed_plant_has_leaf(gui,"step_size")) rpar[i].step_size=weed_get_double_value(gui,"step_size",&error);
          if (weed_plant_has_leaf(gui,"decimals")) rpar[i].dp=weed_get_int_value(gui,"decimals",&error);
        }
        if (rpar[i].step_size==0.) {
          if (rpar[i].max-rpar[i].min>1.) rpar[i].step_size=1.;
          else rpar[i].step_size=1./(double)lives_10pow(rpar[i].dp);
        }
        break;
      case WEED_HINT_TEXT:
        if (weed_plant_has_leaf(wtmpl,"default")&&weed_leaf_num_elements(wtmpl,"default")>1) {
          rpar[i].hidden|=HIDDEN_MULTI;
        }
        rpar[i].type=LIVES_PARAM_STRING;
        if (weed_plant_has_leaf(wtmpl,"host_default")) string=weed_get_string_value(wtmpl,"host_default",&error);
        else if (weed_leaf_num_elements(wtmpl,"default")>0) string=weed_get_string_value(wtmpl,"default",&error);
        else string=weed_get_string_value(wtmpl,"new_default",&error);
        rpar[i].def=lives_strdup(string);
        lives_free(string);
        string=weed_get_string_value(wpar,"value",&error);
        rpar[i].value=lives_strdup(string);
        lives_free(string);
        rpar[i].max=0.;
        if (gui!=NULL&&weed_plant_has_leaf(gui,"maxchars")) {
          rpar[i].max=(double)weed_get_int_value(gui,"maxchars",&error);
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
          rpar[i].value=lives_malloc(3*sizint);
          rpar[i].def=lives_malloc(3*sizint);

          if (weed_leaf_seed_type(wtmpl,"default")==WEED_SEED_INT) {
            if (weed_plant_has_leaf(wtmpl,"host_default")) {
              cols=weed_get_int_array(wtmpl,"host_default",&error);
            } else if (weed_leaf_num_elements(wtmpl,"default")>0) cols=weed_get_int_array(wtmpl,"default",&error);
            else cols=weed_get_int_array(wtmpl,"new_default",&error);
            if (weed_leaf_num_elements(wtmpl,"max")==1) {
              red_max=green_max=blue_max=weed_get_int_value(wtmpl,"max",&error);
            } else {
              maxi=weed_get_int_array(wtmpl,"max",&error);
              red_max=maxi[0];
              green_max=maxi[1];
              blue_max=maxi[2];
            }
            if (weed_leaf_num_elements(wtmpl,"min")==1) {
              red_min=green_min=blue_min=weed_get_int_value(wtmpl,"min",&error);
            } else {
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
            cols[0]=(double)(cols[0]-red_min)/(double)(red_max-red_min)*255.+.49999;
            cols[1]=(double)(cols[1]-green_min)/(double)(green_max-green_min)*255.+.49999;
            cols[2]=(double)(cols[2]-blue_min)/(double)(blue_max-blue_min)*255.+.49999;
            col_int=TRUE;
          } else {
            if (weed_plant_has_leaf(wtmpl,"host_default")) colsd=weed_get_double_array(wtmpl,"host_default",&error);
            else if (weed_leaf_num_elements(wtmpl,"default")>0) colsd=weed_get_double_array(wtmpl,"default",&error);
            else colsd=weed_get_double_array(wtmpl,"default",&error);
            if (weed_leaf_num_elements(wtmpl,"max")==1) {
              red_maxd=green_maxd=blue_maxd=weed_get_double_value(wtmpl,"max",&error);
            } else {
              maxd=weed_get_double_array(wtmpl,"max",&error);
              red_maxd=maxd[0];
              green_maxd=maxd[1];
              blue_maxd=maxd[2];
            }
            if (weed_leaf_num_elements(wtmpl,"min")==1) {
              red_mind=green_mind=blue_mind=weed_get_double_value(wtmpl,"min",&error);
            } else {
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
            cols=(int *)lives_malloc(3*sizshrt);
            cols[0]=(colsd[0]-red_mind)/(red_maxd-red_mind)*255.+.49999;
            cols[1]=(colsd[1]-green_mind)/(green_maxd-green_mind)*255.+.49999;
            cols[2]=(colsd[2]-blue_mind)/(blue_maxd-blue_mind)*255.+.49999;
            col_int=FALSE;
          }
          set_colRGB24_param(rpar[i].def,cols[0],cols[1],cols[2]);

          if (col_int) {
            lives_free(cols);
            cols=weed_get_int_array(wpar,"value",&error);
            if (cols[0]<red_min) cols[0]=red_min;
            if (cols[1]<green_min) cols[1]=green_min;
            if (cols[2]<blue_min) cols[2]=blue_min;
            if (cols[0]>red_max) cols[0]=red_max;
            if (cols[1]>green_max) cols[1]=green_max;
            if (cols[2]>blue_max) cols[2]=blue_max;
            cols[0]=(double)(cols[0]-red_min)/(double)(red_max-red_min)*255.+.49999;
            cols[1]=(double)(cols[1]-green_min)/(double)(green_max-green_min)*255.+.49999;
            cols[2]=(double)(cols[2]-blue_min)/(double)(blue_max-blue_min)*255.+.49999;
          } else {
            colsd=weed_get_double_array(wpar,"value",&error);
            if (colsd[0]<red_mind) colsd[0]=red_mind;
            if (colsd[1]<green_mind) colsd[1]=green_mind;
            if (colsd[2]<blue_mind) colsd[2]=blue_mind;
            if (colsd[0]>red_maxd) colsd[0]=red_maxd;
            if (colsd[1]>green_maxd) colsd[1]=green_maxd;
            if (colsd[2]>blue_maxd) colsd[2]=blue_maxd;
            cols[0]=(colsd[0]-red_mind)/(red_maxd-red_mind)*255.+.49999;
            cols[1]=(colsd[1]-green_mind)/(green_maxd-green_mind)*255.+.49999;
            cols[2]=(colsd[2]-blue_mind)/(blue_maxd-blue_mind)*255.+.49999;
          }
          set_colRGB24_param(rpar[i].value,(short)cols[0],(short)cols[1],(short)cols[2]);
          lives_free(cols);

          if (maxi!=NULL) lives_free(maxi);
          if (mini!=NULL) lives_free(mini);
          if (maxd!=NULL) lives_free(maxd);
          if (mind!=NULL) lives_free(mind);
          break;
        }
        break;

      default:
        rpar[i].type=LIVES_PARAM_UNKNOWN; // TODO - try to get default
      }

      string=weed_get_string_value(wtmpl,"name",&error);
      rpar[i].name=lives_strdup(string);
      rpar[i].label=lives_strdup(string);
      lives_free(string);

      if (weed_plant_has_leaf(wtmpl,"description")) {
        string=weed_get_string_value(wtmpl,"description",&error);
        rpar[i].desc=lives_strdup(string);
        lives_free(string);
      } else rpar[i].desc=NULL;

      // gui part /////////////////////

      if (gui!=NULL) {
        if (weed_plant_has_leaf(gui,"label")) {
          string=weed_get_string_value(gui,"label",&error);
          lives_free(rpar[i].label);
          rpar[i].label=lives_strdup(string);
          lives_free(string);
        }
        if (weed_plant_has_leaf(gui,"use_mnemonic")) rpar[i].use_mnemonic=weed_get_boolean_value(gui,"use_mnemonic",&error);
        if (weed_plant_has_leaf(gui,"hidden"))
          rpar[i].hidden|=((weed_get_boolean_value(gui,"hidden",&error)==WEED_TRUE)*HIDDEN_GUI);
        if (weed_plant_has_leaf(gui,"display_func")) {
          weed_display_f *display_func_ptr_ptr;
          weed_display_f display_func;
          weed_leaf_get(gui,"display_func",0,(void *)&display_func_ptr_ptr);
          display_func=*display_func_ptr_ptr;
          rpar[i].display_func=(fn_ptr)display_func;
        }
        if (weed_plant_has_leaf(gui,"interpolate_func")) {
          weed_interpolate_f *interp_func_ptr_ptr;
          weed_interpolate_f interp_func;
          weed_leaf_get(gui,"interpolate_func",0,(void *)&interp_func_ptr_ptr);
          interp_func=*interp_func_ptr_ptr;
          rpar[i].interp_func=(fn_ptr)interp_func;
        }
      }

      for (j=0; j<MAX_PARAM_WIDGETS; j++) {
        rpar[i].widgets[j]=NULL;
      }
      rpar[i].onchange=FALSE;
    }

    lives_free(wpars);

    return rpar;
  }



  lives_rfx_t *weed_to_rfx(weed_plant_t *plant, boolean show_reinits) {
    // return an RFX for a weed effect; set rfx->source to an INSTANCE of the filter (first instance for compound fx)
    int error;
    weed_plant_t *filter,*inst;

    char *string;
    lives_rfx_t *rfx=(lives_rfx_t *)lives_malloc(sizeof(lives_rfx_t));
    rfx->is_template=FALSE;
    if (weed_get_int_value(plant,"type",&error)==WEED_PLANT_FILTER_INSTANCE) {
      filter=weed_instance_get_filter(plant,TRUE);
      inst=plant;
    } else {
      filter=plant;
      inst=weed_instance_from_filter(filter);
      // init and deinit the effect to allow the plugin to hide parameters, etc.
      weed_reinit_effect(inst,TRUE);
      rfx->is_template=TRUE;
    }

    string=weed_get_string_value(filter,"name",&error);
    rfx->name=lives_strdup(string);
    rfx->menu_text=lives_strdup(string);
    lives_free(string);
    rfx->action_desc=lives_strdup("no action");
    rfx->min_frames=-1;
    rfx->num_in_channels=enabled_in_channels(filter,FALSE);
    rfx->status=RFX_STATUS_WEED;
    rfx->props=0;
    rfx->menuitem=NULL;
    if (!weed_plant_has_leaf(filter,"in_parameter_templates")||
        weed_get_plantptr_value(filter,"in_parameter_templates",&error)==NULL) rfx->num_params=0;
    else rfx->num_params=weed_leaf_num_elements(filter,"in_parameter_templates");
    if (rfx->num_params>0) rfx->params=weed_params_to_rfx(rfx->num_params,inst,show_reinits);
    else rfx->params=NULL;
    rfx->source=(void *)inst;
    rfx->source_type=LIVES_RFX_SOURCE_WEED;
    rfx->extra=NULL;
    return rfx;
  }



  LiVESList *get_external_window_hints(lives_rfx_t *rfx) {
    LiVESList *hints=NULL;

    if (rfx->status==RFX_STATUS_WEED) {
      int nfilters,error;
      int num_hints;
      weed_plant_t *gui;
      weed_plant_t *inst=(weed_plant_t *)rfx->source;
      weed_plant_t *filter=weed_instance_get_filter(inst,TRUE);
      int *filters=NULL;
      char *string,**rfx_strings,*delim;

      register int i;

      if ((nfilters=num_compound_fx(filter))>1) {
        // handle compound fx
        filters=weed_get_int_array(filter,"host_filter_list",&error);
      }

      for (i=0; i<nfilters; i++) {

        if (filters!=NULL) {
          filter=get_weed_filter(filters[i]);
        }

        if (!weed_plant_has_leaf(filter,"gui")) continue;
        gui=weed_get_plantptr_value(filter,"gui",&error);

        if (!weed_plant_has_leaf(gui,"layout_scheme")) continue;

        string=weed_get_string_value(gui,"layout_scheme",&error);
        if (strcmp(string,"RFX")) {
          lives_free(string);
          continue;
        }
        lives_free(string);

        if (!weed_plant_has_leaf(gui,"rfx_delim")) continue;
        delim=weed_get_string_value(gui,"rfx_delim",&error);
        lives_snprintf(rfx->delim,2,"%s",delim);
        lives_free(delim);

        if (!weed_plant_has_leaf(gui,"rfx_strings")) continue;

        num_hints=weed_leaf_num_elements(gui,"rfx_strings");

        if (num_hints==0) continue;
        rfx_strings=weed_get_string_array(gui,"rfx_strings",&error);

        for (i=0; i<num_hints; i++) {
          hints=lives_list_append(hints,lives_strdup(rfx_strings[i]));
          lives_free(rfx_strings[i]);
        }
        lives_free(rfx_strings);

        if (filters!=NULL) hints=lives_list_append(hints,lives_strdup("internal|nextfilter"));

      }

      if (filters!=NULL) lives_free(filters);

    }

    return hints;
  }






  char *plugin_run_param_window(const char *get_com, LiVESVBox *vbox, lives_rfx_t **ret_rfx) {

    // here we create an rfx script from some fixed values and values from the plugin;
    // we will then compile the script to an rfx scrap and use the scrap to get info
    // about additional parameters, and create the parameter window

    // this is done like so to allow use of plugins written in any language;
    // they need only output an RFX scriptlet on stdout when called from the commandline


    // the param window is run, and the marshalled values are returned

    // if the user closes the window with Cancel, NULL is returned instead

    // in parameters: get_com - command to be run to produce rfx output on stdout
    //              : vbox - a vbox where we will display the parameters
    // out parameters: ret_rfx - the value is set to point to an rfx_t effect

    // the string which is returned is the marshalled values of the parameters


    FILE *sfile;

    lives_rfx_t *rfx=(lives_rfx_t *)lives_malloc(sizeof(lives_rfx_t));

    char *string;
    char *rfx_scrapname=lives_strdup_printf("rfx.%d",capable->mainpid);
    char *rfxfile=lives_strdup_printf("%s/.%s.script",prefs->tmpdir,rfx_scrapname);
    char *com;
    char *fnamex;
    char *res_string=NULL;
    char buff[32];

    int res;
    int retval;

    string=lives_strdup_printf("<name>\n%s\n</name>\n",rfx_scrapname);

    do {
      retval=0;
      sfile=fopen(rfxfile,"w");
      if (sfile==NULL) {
        retval=do_write_failed_error_s_with_retry(rfxfile,lives_strerror(errno),NULL);
        if (retval==LIVES_RESPONSE_CANCEL) {
          lives_free(string);
          return NULL;
        }
      } else {
        mainw->write_failed=FALSE;
        lives_fputs(string,sfile);
        fclose(sfile);
        lives_free(string);
        if (mainw->write_failed) {
          retval=do_write_failed_error_s_with_retry(rfxfile,NULL,NULL);
          if (retval==LIVES_RESPONSE_CANCEL) return NULL;
        }
      }
    } while (retval==LIVES_RESPONSE_RETRY);



    com=lives_strdup_printf("%s >> \"%s\"", get_com, rfxfile);
    retval=lives_system(com,FALSE);
    lives_free(com);

    // command failed
    if (retval) return NULL;

    // OK, we should now have an RFX fragment in a file, we can compile it, then build a parameter window from it

    // call RFX_BUILDER program to compile the script, passing parameters input_filename and output_directory
#ifndef IS_MINGW
    com=lives_strdup_printf("\"%s\" \"%s\" \"%s\" >/dev/null",RFX_BUILDER,rfxfile,prefs->tmpdir);
#else
    com=lives_strdup_printf("\"%s\" \"%s\" \"%s\" >NUL",RFX_BUILDER,rfxfile,prefs->tmpdir);
#endif
    res=system(com);
    lives_free(com);

    unlink(rfxfile);
    lives_free(rfxfile);

    if (res==0) {
      // the script compiled correctly

      // now we pop up the parameter window, get the values of our parameters, and marshall them as extra_params

      // first create a lives_rfx_t from the scrap
      rfx->name=lives_strdup(rfx_scrapname);
      rfx->action_desc=NULL;
      rfx->extra=NULL;
      rfx->status=RFX_STATUS_SCRAP;

      rfx->num_in_channels=0;
      rfx->min_frames=-1;

      // get the delimiter
      rfxfile=lives_strdup_printf("%ssmdef.%d",prefs->tmpdir,capable->mainpid);
      fnamex=lives_build_filename(prefs->tmpdir,rfx_scrapname,NULL);
      com=lives_strdup_printf("\"%s\" get_define > \"%s\"",fnamex,rfxfile);
      lives_free(fnamex);
      retval=lives_system(com,FALSE);
      lives_free(com);

      // command to get_define failed
      if (retval) return NULL;

      do {
        retval=0;
        sfile=fopen(rfxfile,"r");
        if (!sfile) {
          retval=do_read_failed_error_s_with_retry(rfxfile,lives_strerror(errno),NULL);
        } else {
          mainw->read_failed=FALSE;
          lives_fgets(buff,32,sfile);
          fclose(sfile);
        }
        if (mainw->read_failed) {
          retval=do_read_failed_error_s_with_retry(rfxfile,NULL,NULL);
        }

      } while (retval==LIVES_RESPONSE_RETRY);

      unlink(rfxfile);
      lives_free(rfxfile);

      if (retval==LIVES_RESPONSE_CANCEL) return NULL;

      lives_snprintf(rfx->delim,2,"%s",buff);

      // ok, this might need adjusting afterwards
      rfx->menu_text=(vbox==NULL?lives_strdup_printf(_("%s advanced settings"),prefs->encoder.of_desc):lives_strdup(""));
      rfx->is_template=FALSE;

      rfx->source=NULL;
      rfx->source_type=LIVES_RFX_SOURCE_RFX;

      render_fx_get_params(rfx,rfx_scrapname,RFX_STATUS_SCRAP);

      // now we build our window and get param values
      if (vbox==NULL) {
        on_fx_pre_activate(rfx,1,NULL);

        if (prefs->show_gui) {
          if (mainw->multitrack==NULL) lives_window_set_transient_for(LIVES_WINDOW(fx_dialog[1]),LIVES_WINDOW(mainw->LiVES));
          else lives_window_set_transient_for(LIVES_WINDOW(fx_dialog[1]),LIVES_WINDOW(mainw->multitrack->window));
        }
        lives_window_set_modal(LIVES_WINDOW(fx_dialog[1]), TRUE);

        if (lives_dialog_run(LIVES_DIALOG(fx_dialog[1]))==LIVES_RESPONSE_OK) {
          // marshall our params for passing to the plugin
          res_string=param_marshall(rfx,FALSE);
        }
      } else {
        make_param_box(vbox,rfx);
      }

      rfxfile=lives_build_filename(prefs->tmpdir,rfx_scrapname,NULL);
      unlink(rfxfile);
      lives_free(rfxfile);

      if (ret_rfx!=NULL) {
        *ret_rfx=rfx;
      } else {
        rfx_free(rfx);
        lives_free(rfx);
      }
    } else {
      if (ret_rfx!=NULL) {
        *ret_rfx=NULL;
      } else {
        res_string=lives_strdup("");
      }
      if (rfx!=NULL) {
        lives_free(rfx);
      }
    }

    lives_free(rfx_scrapname);
    return res_string;
  }
