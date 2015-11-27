// yuv4mpeg.c
// LiVES
// (c) G. Finch 2004 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-host.h>
#include <weed/weed-palettes.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#endif

#include "main.h"

#include "support.h"
#include "interface.h"
#include "lives-yuv4mpeg.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

static boolean gotbroken;

typedef struct y4data {
  const char *filename;
  lives_yuv4m_t *yuv4mpeg;

  int fd;
  int i;

} y4data;


static int yuvout,hsize_out,vsize_out;


// lists of cards in use
static LiVESList *fw_cards=NULL;


static lives_yuv4m_t *lives_yuv4mpeg_alloc(void) {
  lives_yuv4m_t *yuv4mpeg = (lives_yuv4m_t *) malloc(sizeof(lives_yuv4m_t));
  if (yuv4mpeg==NULL) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  y4m_init_stream_info(&(yuv4mpeg->streaminfo));
  y4m_init_frame_info(&(yuv4mpeg->frameinfo));
  yuv4mpeg->filename=NULL;
  yuv4mpeg->name=NULL;
  yuv4mpeg->fd=-1;
  yuv4mpeg->ready=FALSE;
  return yuv4mpeg;
}


static void *y4open_thread(void *arg) {
  y4data *thread_data=(y4data *)arg;
  int fd=open(thread_data->filename,O_RDONLY);
  thread_data->fd=fd;
  pthread_exit(NULL);
}


static void *y4header_thread(void *arg) {
  y4data *thread_data=(y4data *)arg;
  lives_yuv4m_t *yuv4mpeg=thread_data->yuv4mpeg;
  thread_data->i = y4m_read_stream_header(yuv4mpeg->fd, &(yuv4mpeg->streaminfo));
  pthread_exit(NULL);
}


static void fill_read(int fd, char *buf, size_t count) {
  size_t bytes=0;
  ssize_t got;

  do {
    got=read(fd,buf+bytes,count-bytes);
    if (got<0) return;
    bytes+=got;
  } while (bytes<count);

}


static void *y4frame_thread(void *arg) {
  y4data *thread_data=(y4data *)arg;
  lives_yuv4m_t *yuv4mpeg=thread_data->yuv4mpeg;
  char buff[5],bchar;

  thread_data->i=Y4M_OK;

  // read 5 bytes FRAME
  fill_read(yuv4mpeg->fd,buff,5);

  if (strncmp(buff,"FRAME",5)) {
    if (!gotbroken) {
      thread_data->i=Y4M_ERR_MAGIC;
      pthread_exit(NULL);
    }

    do {
      memmove(buff,buff+1,4);
      fill_read(yuv4mpeg->fd,buff+4,1);
    } while (strncmp(buff,"FRAME",5));

  }

  do {
    fill_read(yuv4mpeg->fd,&bchar,1);
  } while (strncmp(&bchar,"\n",1));

  // read YUV420
  fill_read(yuv4mpeg->fd,(char *)yuv4mpeg->pixel_data[0],yuv4mpeg->hsize*yuv4mpeg->vsize);
  fill_read(yuv4mpeg->fd,(char *)yuv4mpeg->pixel_data[1],yuv4mpeg->hsize*yuv4mpeg->vsize/4);
  fill_read(yuv4mpeg->fd,(char *)yuv4mpeg->pixel_data[2],yuv4mpeg->hsize*yuv4mpeg->vsize/4);

  pthread_exit(NULL);
}



#define YUV4_O_TIME 200000000 // ticks to wait to open fifo
#define YUV4_H_TIME 500000000 // ticks to wait to get stream header


static boolean lives_yuv_stream_start_read(lives_clip_t *sfile) {
  double ofps=sfile->fps;

  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)sfile->ext_src;

  pthread_t y4thread;

  char *filename=yuv4mpeg->filename,*tmp;

  int alarm_handle=0;

  int ohsize=sfile->hsize;
  int ovsize=sfile->vsize;

  y4data thread_data;

  register int i;


  if (filename==NULL) return FALSE;

  if (yuv4mpeg->fd==-1) {
    // create a thread to open the fifo

    thread_data.filename=filename;

    pthread_create(&y4thread,NULL,y4open_thread,(void *)&thread_data);

    alarm_handle=lives_alarm_set(YUV4_O_TIME);

    d_print("");
    d_print(_("Waiting for yuv4mpeg frames..."));

    gotbroken=FALSE;

    while (!lives_alarm_get(alarm_handle)&&!pthread_kill(y4thread,0)) {
      // wait for thread to complete or timeout
      lives_usleep(prefs->sleep_time);
      lives_widget_context_update();
    }

    if (lives_alarm_get(alarm_handle)) {
      // timeout - kill thread and wait for it to terminate
      pthread_cancel(y4thread);
      pthread_join(y4thread,NULL);
      lives_alarm_clear(alarm_handle);

      d_print_failed();
      d_print(_("Unable to open the incoming video stream\n"));

      yuv4mpeg->fd=thread_data.fd;

      if (yuv4mpeg->fd>=0) {
        close(yuv4mpeg->fd);
        yuv4mpeg->fd=-1;
      }

      return FALSE;
    }

    pthread_join(y4thread,NULL);
    lives_alarm_clear(alarm_handle);

    yuv4mpeg->fd=thread_data.fd;

    if (yuv4mpeg->fd<0) {
      return FALSE;
    }
  }

  // create a thread to open the stream header
  thread_data.yuv4mpeg=yuv4mpeg;
  pthread_create(&y4thread,NULL,y4header_thread,&thread_data);
  alarm_handle=lives_alarm_set(YUV4_H_TIME);

  while (!lives_alarm_get(alarm_handle)&&!pthread_kill(y4thread,0)) {
    // wait for thread to complete or timeout
    lives_usleep(prefs->sleep_time);
    lives_widget_context_update();
  }

  if (lives_alarm_get(alarm_handle)) {
    // timeout - kill thread and wait for it to terminate
    pthread_cancel(y4thread);
    pthread_join(y4thread,NULL);
    lives_alarm_clear(alarm_handle);
    d_print(_("Unable to read the stream header\n"));
    return FALSE;
  }

  pthread_join(y4thread,NULL);
  lives_alarm_clear(alarm_handle);

  i=thread_data.i;

  if (i != Y4M_OK) {
    char *tmp;
    d_print((tmp=lives_strdup_printf("yuv4mpeg: %s\n", y4m_strerr(i))));
    lives_free(tmp);
    return FALSE;
  }

  d_print(_("got header\n"));

  sfile->hsize = yuv4mpeg->hsize = y4m_si_get_width(&(yuv4mpeg->streaminfo));
  sfile->vsize = yuv4mpeg->vsize = y4m_si_get_height(&(yuv4mpeg->streaminfo));

  sfile->fps=cfile->pb_fps=lives_strtod(lives_strdup_printf("%.8f",Y4M_RATIO_DBL
                                        (y4m_si_get_framerate(&(yuv4mpeg->streaminfo)))),NULL);

  if (!(sfile->hsize*sfile->vsize)) {
    do_error_dialog(lives_strdup_printf(_("Video dimensions: %d x %d are invalid. Stream cannot be opened"),
                                        sfile->hsize,sfile->vsize));
    return FALSE;
  }

  if (sfile->hsize!=ohsize||sfile->vsize!=ovsize||sfile->fps!=ofps) {
    set_main_title(sfile->file_name,0);
  }

  d_print((tmp=lives_strdup_printf(_("Reset clip values for %s: size=%dx%d fps=%.3f\n"),yuv4mpeg->name,
                                   cfile->hsize,yuv4mpeg->vsize,cfile->bpp,cfile->fps)));
  lives_free(tmp);

  yuv4mpeg->ready=TRUE;

  return TRUE;
}



void lives_yuv_stream_stop_read(lives_yuv4m_t *yuv4mpeg) {

  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  if (yuv4mpeg->fd!=-1) close(yuv4mpeg->fd);

  if (yuv4mpeg->filename!=NULL) {
    unlink(yuv4mpeg->filename);
    lives_free(yuv4mpeg->filename);
  }

  if (yuv4mpeg->name!=NULL) lives_free(yuv4mpeg->name);

  if (yuv4mpeg->type==YUV4_TYPE_FW) fw_cards=lives_list_remove(fw_cards,LIVES_INT_TO_POINTER(yuv4mpeg->cardno));
  if (yuv4mpeg->type==YUV4_TYPE_TV) mainw->videodevs=lives_list_remove(mainw->videodevs,LIVES_INT_TO_POINTER(yuv4mpeg->cardno));


}

#define YUV4_F_TIME 200000000 // ticks to wait to get stream header


void weed_layer_set_from_yuv4m(weed_plant_t *layer, lives_clip_t *sfile) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)(sfile->ext_src);

  y4data thread_data;

  int error;

  pthread_t y4thread;

  int alarm_handle;

  if (!yuv4mpeg->ready) lives_yuv_stream_start_read(sfile);

  weed_set_int_value(layer,"width",sfile->hsize);
  weed_set_int_value(layer,"height",sfile->vsize);
  weed_set_int_value(layer,"current_palette",WEED_PALETTE_YUV420P);
  weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);

  create_empty_pixel_data(layer,TRUE,TRUE);

  if (!yuv4mpeg->ready) {
    return;
  }

  yuv4mpeg->pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  // create a thread to open the stream header

  thread_data.yuv4mpeg=yuv4mpeg;
  pthread_create(&y4thread,NULL,y4frame_thread,&thread_data);

  alarm_handle=lives_alarm_set(YUV4_F_TIME);

  while (!lives_alarm_get(alarm_handle)&&!pthread_kill(y4thread,0)) {
    // wait for thread to complete or timeout
    lives_usleep(prefs->sleep_time);
  }

  if (lives_alarm_get(alarm_handle)) {
    // timeout - kill thread and wait for it to terminate
    // timeout - kill thread and wait for it to terminate
    pthread_cancel(y4thread);
    d_print(_("Unable to read the incoming video frame\n"));
    gotbroken=TRUE;
  } else gotbroken=FALSE;

  pthread_join(y4thread,NULL);
  lives_alarm_clear(alarm_handle);

  lives_free(yuv4mpeg->pixel_data);
  yuv4mpeg->pixel_data=NULL;

  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  return;
}




static boolean open_yuv4m_inner(const char *filename, const char *fname, int new_file, int type, int cardno) {
  // create a virtual clip
  int old_file=mainw->current_file;

  lives_yuv4m_t *yuv4mpeg;

  cfile->clip_type=CLIP_TYPE_YUV4MPEG;

  // get size of frames, arate, achans, asamps, signed endian
  yuv4mpeg=lives_yuv4mpeg_alloc();

  yuv4mpeg->fd=-1;

  yuv4mpeg->filename=lives_strdup(filename);
  yuv4mpeg->name=lives_strdup(fname);

  yuv4mpeg->type=type;
  yuv4mpeg->cardno=cardno;

  cfile->ext_src=yuv4mpeg;

  cfile->bpp = 12;

  cfile->start=cfile->end=cfile->frames=1;

  cfile->hsize=DEF_GEN_WIDTH;
  cfile->vsize=DEF_GEN_HEIGHT;

  cfile->is_loaded=TRUE;

  add_to_clipmenu();

  switch_to_file((mainw->current_file=old_file),new_file);

  return TRUE;
}





void on_open_yuv4m_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  // open a general yuvmpeg stream
  // start "playing" but open frames in yuv4mpeg format on stdin

  int old_file=mainw->current_file,new_file=mainw->first_free_file;
  char *tmp;
  char *filename;
  char *fname;

  char *audio_real,*audio_fake;

  if (menuitem && !do_yuv4m_open_warning()) return;

  fname=lives_strdup(_("yuv4mpeg stream"));

  if (!get_new_handle(new_file,fname)) {
    lives_free(fname);
    return;
  }

  mainw->current_file=new_file;

  if (!strlen(prefs->yuvin))
    filename=lives_build_filename(prefs->tmpdir,"stream.yuv",NULL);
  else
    filename=lives_strdup(prefs->yuvin);

  mkfifo(filename,S_IRUSR|S_IWUSR);

  if (!open_yuv4m_inner(filename,fname,new_file,YUV4_TYPE_GENERIC,0)) {
    close_current_file(old_file);
    lives_free(filename);
    lives_free(fname);
    return;
  }

  lives_free(fname);

  if (!lives_yuv_stream_start_read(cfile)) {
    close_current_file(old_file);
    lives_free(filename);
    return;
  }

  new_file=mainw->current_file;

  lives_snprintf(cfile->type,40,"%s",_("yu4mpeg stream in"));

  d_print((tmp=lives_strdup_printf(_("Opened yuv4mpeg stream on %s"),filename)));
  lives_free(tmp);
  lives_free(filename);

  d_print(_("Audio: "));

  if (cfile->achans==0) {
    d_print(_("none\n"));
  } else {
    d_print((tmp=lives_strdup_printf(P_("%d Hz %d channel %d bps\n","%d Hz %d channels %d bps\n",cfile->achans),
                                     cfile->arate,cfile->achans,cfile->asampsize)));
    lives_free(tmp);
  }

  // if not playing, start playing
  if (mainw->playing_file==-1) {

    // temp kludge, symlink audiodump.pcm to wav file, then pretend we are playing
    // an opening preview . Doesn't work with fifo.
    // and we dont really care if it doesnt work

    // but what it means is, if we have an audio file or stream at
    // "prefs->tmpdir/audiodump.pcm" we will try to play it



    // real is tmpdir/audiodump.pcm
    audio_real=lives_build_filename(prefs->tmpdir,"audiodump.pcm",NULL);
    // fake is tmpdir/handle/audiodump.pcm
    audio_fake=lives_build_filename(prefs->tmpdir,cfile->handle,"audiodump.pcm",NULL);


#ifndef IS_MINGW
    // fake file will go away when we close the current clip
    lives_system((tmp=lives_strdup_printf("%s -s \"%s\" \"%s\" >/dev/null 2>&1",capable->ln_cmd,
                                          audio_real,audio_fake)),TRUE);
#else
    // TODO
#endif

    lives_free(audio_real);
    lives_free(audio_fake);

    lives_free(tmp);

    // start playing
    play_file();

    mainw->noswitch=FALSE;
  }
  // TODO - else...

  if (mainw->current_file!=old_file&&mainw->current_file!=new_file)
    old_file=mainw->current_file; // we could have rendered to a new file

  mainw->current_file=new_file;

  // close this temporary clip
  close_current_file(old_file);

}


///////////////////////////////////////////////////////////////////////////////
// write functions - not used currently


boolean lives_yuv_stream_start_write(lives_yuv4m_t *yuv4mpeg, const char *filename, int hsize, int vsize, double fps) {
  int i;

  if (mainw->fixed_fpsd>-1.&&mainw->fixed_fpsd!=fps) {
    do_error_dialog(lives_strdup_printf(_("Unable to set display framerate to %.3f fps.\n\n"),fps));
    return FALSE;
  }
  mainw->fixed_fpsd=fps;

  if (filename==NULL) filename=lives_strdup_printf("%s/streamout.yuv",prefs->tmpdir);

  // TODO - do_threaded_dialog
  if ((yuvout=creat(filename,O_CREAT))<0) {
    do_error_dialog(lives_strdup_printf(_("Unable to open yuv4mpeg out stream %s\n"),filename));
    return FALSE;
  }

  if (mainw->fixed_fpsd>23.9999&&mainw->fixed_fpsd<24.0001) {
    y4m_si_set_framerate(&(yuv4mpeg->streaminfo),y4m_fps_FILM);
  } else return FALSE;
  y4m_si_set_interlace(&(yuv4mpeg->streaminfo), Y4M_ILACE_NONE);

  y4m_si_set_width(&(yuv4mpeg->streaminfo), (hsize_out=hsize));
  y4m_si_set_height(&(yuv4mpeg->streaminfo), (vsize_out=vsize));
  y4m_si_set_sampleaspect(&(yuv4mpeg->streaminfo), yuv4mpeg->sar);

  i = y4m_write_stream_header(yuvout, &(yuv4mpeg->streaminfo));

  if (i != Y4M_OK) return FALSE;

  return TRUE;
}


boolean lives_yuv_stream_write_frame(lives_yuv4m_t *yuv4mpeg, void *pixel_data) {
  // pixel_data is planar yuv420 data
  int i;

  uint8_t *planes[3];
  uint8_t *pixels=(uint8_t *)pixel_data;

  planes[0]=&(pixels[0]);
  planes[1]=&(pixels[hsize_out*vsize_out]);
  planes[2]=&(pixels[hsize_out*vsize_out*5/4]);

  i = y4m_write_frame(yuvout, &(yuv4mpeg->streaminfo),
                      &(yuv4mpeg->frameinfo), (uint8_t **)&planes[0]);
  if (i != Y4M_OK) return FALSE;
  return TRUE;
}


void lives_yuv_stream_stop_write(lives_yuv4m_t *yuv4mpeg) {
  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));
  close(yuvout);
}




//////////////////////////////////////////////////////////////

// add live input peripherals

// some time in the future it would be nice to implement these via videojack

// advantages would be: - no longer necessary to have mjpegtools
// - multiple copies of LiVES could share the same input at (almost) zero cost



// for each of thes functions:
// - prompt user for name of device, etc.

// check if device already opened, if so exit

// create clip with default values; clip type is YUV4MPEG

// create a fifo file
// set mplayer reading from device and writing yuv4mpeg

// start reading - update clip values

// note: we add the clip to the menu and to mainw->cliplist
// beware when handling mainw->cliplist





void on_live_tvcard_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int cardno=0;

  int new_file=mainw->first_free_file;

  int response;

  char *com,*tmp;
  char *fifofile=lives_strdup_printf("%s/tvpic_%d.y4m",prefs->tmpdir,capable->mainpid);

  char *chanstr;
  char *devstr;

  char *fname;

  LiVESWidget *card_dialog;

  lives_tvcardw_t *tvcardw;

  mainw->open_deint=FALSE;

  card_dialog=create_cdtrack_dialog(4,NULL);

  tvcardw=(lives_tvcardw_t *)lives_widget_object_get_data(LIVES_WIDGET_OBJECT(card_dialog),"tvcard_data");


  response=lives_dialog_run(LIVES_DIALOG(card_dialog));
  if (response==LIVES_RESPONSE_CANCEL) {
    lives_widget_destroy(card_dialog);
    lives_free(fifofile);
    lives_free(tvcardw);
    return;
  }

  cardno=(int)mainw->fx1_val;
  chanstr=lives_strdup_printf("%d",(int)mainw->fx2_val);

  if (lives_list_find(mainw->videodevs,LIVES_INT_TO_POINTER(cardno))) {
    lives_widget_destroy(card_dialog);
    do_card_in_use_error();
    lives_free(chanstr);
    lives_free(fifofile);
    lives_free(tvcardw);
    return;
  }

  fname=lives_strdup_printf(_("TV card %d"),cardno);

  if (!get_new_handle(new_file,fname)) {
    lives_widget_destroy(card_dialog);
    lives_free(chanstr);
    lives_free(fifofile);
    lives_free(fname);
    lives_free(tvcardw);
    return;
  }

  devstr=lives_strdup_printf("/dev/video%d",cardno);

  if (!check_dev_busy(devstr)) {
    lives_widget_destroy(card_dialog);
    do_dev_busy_error(fname);
    lives_free(devstr);
    lives_free(chanstr);
    lives_free(fifofile);
    lives_free(fname);
    lives_free(tvcardw);
    return;
  }

  mainw->videodevs=lives_list_append(mainw->videodevs,LIVES_INT_TO_POINTER(cardno));

  mainw->current_file=new_file;

  cfile->deinterlace=mainw->open_deint;

  unlink(fifofile);
  mkfifo(fifofile,S_IRUSR|S_IWUSR);

  if (!tvcardw->use_advanced) {
    com=lives_strdup_printf("%s open_tv_card \"%s\" \"%s\" \"%s\" \"%s\"",prefs->backend,cfile->handle,chanstr,
                            devstr,fifofile);
  } else {
    double fps=0.;
    char *driver=NULL,*outfmt=NULL;
    int width=0,height=0;
    int input=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(tvcardw->spinbuttoni));

    if (!lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(tvcardw->radiobuttond))) {
      width=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(tvcardw->spinbuttonw));
      height=lives_spin_button_get_value_as_int(LIVES_SPIN_BUTTON(tvcardw->spinbuttonh));
      fps=lives_spin_button_get_value(LIVES_SPIN_BUTTON(tvcardw->spinbuttonf));
    }

    driver=lives_combo_get_active_text(LIVES_COMBO(tvcardw->combod));
    outfmt=lives_combo_get_active_text(LIVES_COMBO(tvcardw->comboo));

    com=lives_strdup_printf("%s open_tv_card \"%s\" \"%s\" \"%s\" \"%s\" %d %d %d %.3f \"%s\" \"%s\"",
                            prefs->backend,cfile->handle,chanstr,
                            devstr,fifofile,input,width,height,fps,driver,outfmt);
    lives_free(driver);
    lives_free(outfmt);

  }
  lives_widget_destroy(card_dialog);
  lives_free(tvcardw);

  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    lives_free(fname);
    lives_free(chanstr);
    lives_free(fifofile);
    lives_free(devstr);
    return;
  }

  if (!open_yuv4m_inner(fifofile,fname,new_file,YUV4_TYPE_TV,cardno)) {
    lives_free(fname);
    lives_free(chanstr);
    lives_free(fifofile);
    lives_free(devstr);
    return;
  }

  lives_snprintf(cfile->type,40,"%s",fname);

  d_print((tmp=lives_strdup_printf(_("Opened TV card %d (%s)"),cardno,devstr)));

  lives_free(tmp);
  lives_free(fname);
  lives_free(chanstr);
  lives_free(devstr);
  lives_free(fifofile);

}



void on_live_fw_activate(LiVESMenuItem *menuitem, livespointer user_data) {

  char *com,*tmp;
  int cardno;
  int cache=1024;

  int new_file=mainw->first_free_file;

  int response;

  char *fifofile=lives_strdup_printf("%s/firew_%d.y4m",prefs->tmpdir,capable->mainpid);
  char *fname;

  LiVESWidget *card_dialog;

  mainw->open_deint=FALSE;

  card_dialog=create_cdtrack_dialog(5,NULL);
  response=lives_dialog_run(LIVES_DIALOG(card_dialog));
  if (response==LIVES_RESPONSE_CANCEL) {
    lives_widget_destroy(card_dialog);
    lives_free(fifofile);
    return;
  }

  cardno=(int)mainw->fx1_val;

  lives_widget_destroy(card_dialog);

  if (lives_list_find(fw_cards,LIVES_INT_TO_POINTER(cardno))) {
    lives_free(fifofile);
    do_card_in_use_error();
    return;
  }

  fname=lives_strdup_printf(_("Firewire card %d"),cardno);

  if (!get_new_handle(new_file,fname)) {
    lives_free(fifofile);
    lives_free(fname);
    return;
  }

  fw_cards=lives_list_append(fw_cards,LIVES_INT_TO_POINTER(cardno));

  mainw->current_file=new_file;
  cfile->deinterlace=mainw->open_deint;

  unlink(fifofile);
  mkfifo(fifofile,S_IRUSR|S_IWUSR);

  com=lives_strdup_printf("%s open_fw_card \"%s\" %d %d \"%s\"",prefs->backend,cfile->handle,cardno,cache,fifofile);
  mainw->com_failed=FALSE;
  lives_system(com,FALSE);
  lives_free(com);

  if (mainw->com_failed) {
    mainw->com_failed=FALSE;
    lives_free(fname);
    lives_free(fifofile);
    return;
  }

  if (!open_yuv4m_inner(fifofile,fname,new_file,YUV4_TYPE_FW,cardno)) {
    lives_free(fname);
    lives_free(fifofile);
    return;
  }

  lives_snprintf(cfile->type,40,"%s",fname);

  d_print((tmp=lives_strdup_printf(_("Opened firewire card %d"),cardno)));

  lives_free(tmp);
  lives_free(fname);
  lives_free(fifofile);

}
