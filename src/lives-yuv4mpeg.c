// yuv4mpeg.c
// LiVES
// (c) G. Finch 2004 - 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifdef HAVE_SYSTEM_WEED
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
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

static int yuvout,hsize_out,vsize_out;


// lists of cards in use
static GList *fw_cards=NULL;


static lives_yuv4m_t *lives_yuv4mpeg_alloc (void) {
  lives_yuv4m_t *yuv4mpeg = (lives_yuv4m_t *) malloc (sizeof(lives_yuv4m_t));
  if (yuv4mpeg==NULL) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  y4m_init_stream_info (&(yuv4mpeg->streaminfo));
  y4m_init_frame_info (&(yuv4mpeg->frameinfo));
  yuv4mpeg->filename=NULL;
  yuv4mpeg->name=NULL;
  yuv4mpeg->fd=-1;
  yuv4mpeg->ready=FALSE;
  return yuv4mpeg;
}


static void *y4open_thread (void *arg) {
  char *filename=(char *)arg;
  int fd=open (filename,O_RDONLY);
  pthread_exit(GINT_TO_POINTER(fd));
}


static void *y4header_thread (void *arg) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)arg;
  int i = y4m_read_stream_header (yuv4mpeg->fd, &(yuv4mpeg->streaminfo));
  pthread_exit(GINT_TO_POINTER(i));
}


static void fill_read(int fd, void *buf, size_t count) {
  size_t bytes=0;

  do {
    bytes+=read(fd,buf+bytes,count-bytes);
  } while (bytes<count);

}


static void *y4frame_thread (void *arg) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)arg;
  gchar buff[5],bchar;
  int i=Y4M_OK;

  // read 5 bytes FRAME
  fill_read(yuv4mpeg->fd,&buff,5);

  if (strncmp(buff,"FRAME",5)) {
    i=Y4M_ERR_MAGIC;
    pthread_exit(GINT_TO_POINTER(i));
  }

  do {
    fill_read(yuv4mpeg->fd,&bchar,1);
  } while (strncmp(&bchar,"\n",1));

  // read YUV420
  fill_read(yuv4mpeg->fd,yuv4mpeg->pixel_data[0],yuv4mpeg->hsize*yuv4mpeg->vsize);
  fill_read(yuv4mpeg->fd,yuv4mpeg->pixel_data[1],yuv4mpeg->hsize*yuv4mpeg->vsize/4);
  fill_read(yuv4mpeg->fd,yuv4mpeg->pixel_data[2],yuv4mpeg->hsize*yuv4mpeg->vsize/4);

  pthread_exit(GINT_TO_POINTER(i));
}



#define YUV4_O_TIME 2000000 // micro-seconds to wait to open fifo
#define YUV4_H_TIME 5000000 // micro-seconds to wait to get stream header


static gboolean lives_yuv_stream_start_read (file *sfile) {
  int i;

  int ohsize=sfile->hsize;
  int ovsize=sfile->vsize;
  gdouble ofps=sfile->fps;

  lives_yuv4m_t *yuv4mpeg=sfile->ext_src;

  gchar *filename=yuv4mpeg->filename,*tmp;

  pthread_t y4thread;
  struct timeval otv;
  int64_t ntime=0,stime;

  void *retval;

  if (filename==NULL) return FALSE;

  if (yuv4mpeg->fd==-1) {
    // create a thread to open the fifo
    pthread_create(&y4thread,NULL,y4open_thread,(void *)filename);
    gettimeofday(&otv,NULL);
    stime=otv.tv_sec*1000000+otv.tv_usec;
    

    while (ntime<YUV4_O_TIME&&!pthread_kill(y4thread,0)) {
      // wait for thread to complete or timeout
      g_usleep(prefs->sleep_time);
      while (g_main_context_iteration(NULL,FALSE));
      
      gettimeofday(&otv, NULL);
      ntime=(otv.tv_sec*1000000+otv.tv_usec-stime);
    }
    
    if (ntime>=YUV4_O_TIME) {
      // timeout - kill thread and wait for it to terminate
      pthread_cancel(y4thread);
      pthread_join(y4thread,&retval);
      d_print(_("Unable to open the incoming video stream\n"));

      yuv4mpeg->fd=GPOINTER_TO_INT(retval);
      if (yuv4mpeg->fd!=-1) {
	close(yuv4mpeg->fd);
	yuv4mpeg->fd=-1;
      }

      return FALSE;
    }

    pthread_join(y4thread,&retval);

    yuv4mpeg->fd=GPOINTER_TO_INT(retval);

    if (yuv4mpeg->fd==-1) {
      return FALSE;
    }
  }

  // create a thread to open the stream header
  pthread_create(&y4thread,NULL,y4header_thread,yuv4mpeg);
  gettimeofday(&otv,NULL);
  stime=otv.tv_sec*1000000+otv.tv_usec;

  while (ntime<YUV4_H_TIME&&!pthread_kill(y4thread,0)) {
    // wait for thread to complete or timeout
    g_usleep(prefs->sleep_time);
    while (g_main_context_iteration(NULL,FALSE));

    gettimeofday(&otv, NULL);
    ntime=(otv.tv_sec*1000000+otv.tv_usec-stime);
    
  }

  if (ntime>=YUV4_H_TIME) {
    // timeout - kill thread and wait for it to terminate
    pthread_cancel(y4thread);
    pthread_join(y4thread,NULL);
    d_print(_("Unable to read the incoming video stream\n"));
    return FALSE;
  }

  pthread_join(y4thread,&retval);

  i=GPOINTER_TO_INT(retval);

  if (i != Y4M_OK) {
    gchar *tmp;
    d_print ((tmp=g_strdup_printf ("yuv4mpeg: %s\n", y4m_strerr (i))));
    g_free(tmp);
    return FALSE;
  }

  sfile->hsize = yuv4mpeg->hsize = y4m_si_get_width (&(yuv4mpeg->streaminfo));
  sfile->vsize = yuv4mpeg->vsize = y4m_si_get_height (&(yuv4mpeg->streaminfo));

  sfile->fps=cfile->pb_fps=g_strtod (g_strdup_printf ("%.8f",Y4M_RATIO_DBL (y4m_si_get_framerate (&(yuv4mpeg->streaminfo)))),NULL);

  if(!(sfile->hsize*sfile->vsize)){
      do_error_dialog (g_strdup_printf (_("Video dimensions: %d x %d are invalid. Stream cannot be opened"),sfile->hsize,sfile->vsize));
      return FALSE;
  }

  if (sfile->hsize!=ohsize||sfile->vsize!=ovsize||sfile->fps!=ofps) {
    set_main_title(sfile->file_name,0);
  }

  d_print ((tmp=g_strdup_printf(_ ("Reset clip values for %s: size=%dx%d fps=%.3f\n"),yuv4mpeg->name,cfile->hsize,yuv4mpeg->vsize,cfile->bpp,cfile->fps)));
  g_free(tmp);

  yuv4mpeg->ready=TRUE;

  return TRUE;
}



void lives_yuv_stream_stop_read (lives_yuv4m_t *yuv4mpeg) {

  y4m_fini_stream_info (&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info (&(yuv4mpeg->frameinfo));
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  if (yuv4mpeg->fd!=-1) close (yuv4mpeg->fd);

  if (yuv4mpeg->filename!=NULL) {
    unlink(yuv4mpeg->filename);
    g_free(yuv4mpeg->filename);
  }

  if (yuv4mpeg->name!=NULL) g_free(yuv4mpeg->name);

  if (yuv4mpeg->type==YUV4_TYPE_FW) fw_cards=g_list_remove(fw_cards,GINT_TO_POINTER(yuv4mpeg->cardno));
  if (yuv4mpeg->type==YUV4_TYPE_TV) mainw->videodevs=g_list_remove(mainw->videodevs,GINT_TO_POINTER(yuv4mpeg->cardno));


}

#define YUV4_F_TIME 2000000 // micro-seconds to wait to get stream header


void weed_layer_set_from_yuv4m (weed_plant_t *layer, file *sfile) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)(sfile->ext_src);
  int error;

  pthread_t y4thread;
  struct timeval otv;
  int64_t ntime=0,stime;

  if (!yuv4mpeg->ready) lives_yuv_stream_start_read(sfile);

  weed_set_int_value(layer,"width",sfile->hsize);
  weed_set_int_value(layer,"height",sfile->vsize);
  weed_set_int_value(layer,"current_palette",WEED_PALETTE_YUV420P);
  weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);

  if (!yuv4mpeg->ready) {
    create_empty_pixel_data(layer,FALSE);
    return;
  }

  create_empty_pixel_data(layer,FALSE);

  yuv4mpeg->pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  // create a thread to open the stream header
  pthread_create(&y4thread,NULL,y4frame_thread,yuv4mpeg);
  gettimeofday(&otv,NULL);
  stime=otv.tv_sec*1000000+otv.tv_usec;

  while (ntime<YUV4_F_TIME&&!pthread_kill(y4thread,0)) {
    // wait for thread to complete or timeout
    g_usleep(prefs->sleep_time);
    gettimeofday(&otv, NULL);
    ntime=(otv.tv_sec*1000000+otv.tv_usec-stime);
  }

  if (ntime>=YUV4_F_TIME) {
    // timeout - kill thread and wait for it to terminate
    pthread_cancel(y4thread);
    d_print(_("Unable to read the incoming video frame\n"));
  }

  pthread_join(y4thread,NULL);

  weed_free(yuv4mpeg->pixel_data);
  yuv4mpeg->pixel_data=NULL;

  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  return;
}




static gboolean open_yuv4m_inner(const gchar *filename, const gchar *fname, gint new_file, gint type, gint cardno) {
  // create a virtual clip
  gint old_file=mainw->current_file;

  lives_yuv4m_t *yuv4mpeg;

  cfile->clip_type=CLIP_TYPE_YUV4MPEG;

  // get size of frames, arate, achans, asamps, signed endian
  yuv4mpeg=lives_yuv4mpeg_alloc();

  yuv4mpeg->fd=-1;

  yuv4mpeg->filename=g_strdup(filename);
  yuv4mpeg->name=g_strdup(fname);

  yuv4mpeg->type=type;
  yuv4mpeg->cardno=cardno;

  cfile->ext_src=yuv4mpeg;

  cfile->bpp = 12;

  cfile->start=cfile->end=cfile->frames=1;

  cfile->hsize=DEF_GEN_WIDTH;
  cfile->vsize=DEF_GEN_HEIGHT;

  cfile->is_loaded=TRUE;

  add_to_winmenu();

  switch_to_file((mainw->current_file=old_file),new_file);

  return TRUE;
}





void on_open_yuv4m_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // open a general yuvmpeg stream
  // start "playing" but open frames in yuv4mpeg format on stdin

  gint old_file=mainw->current_file,new_file=mainw->first_free_file;
  gchar *tmp;
  gchar *filename;
  gchar *fname;

  if (!do_yuv4m_open_warning()) return;

  fname=g_strdup(_("yuv4mpeg stream"));

  if (!get_new_handle(new_file,fname)) {
    g_free(fname);
    return;
  }

  mainw->current_file=new_file;

  filename=g_strdup_printf("%s/stream.yuv",prefs->tmpdir);
  mkfifo(filename,S_IRUSR|S_IWUSR);

  if (!open_yuv4m_inner(filename,fname,new_file,YUV4_TYPE_GENERIC,0)) {
    close_current_file(old_file);
    g_free(filename);
    g_free(fname);
    return;
  }

  g_free(fname);

  if (!lives_yuv_stream_start_read(cfile)) {
    close_current_file(old_file);
    g_free(filename);
    return;
  }

  new_file=mainw->current_file;

  g_snprintf(cfile->type,40,"%s",_("yu4mpeg stream in"));

  d_print ((tmp=g_strdup_printf (_("Opened yuv4mpeg stream on %s"),filename)));
  g_free(tmp);
  g_free(filename);

  d_print(_("Audio: "));

  if (cfile->achans==0) {
    d_print (_ ("none\n"));
  }
  else {
    d_print ((tmp=g_strdup_printf(_ ("%d Hz %d channel(s) %d bps\n"),cfile->arate,cfile->achans,cfile->asampsize)));
    g_free(tmp);
  }

  // if not playing, start playing
  if (mainw->playing_file==-1) {
    if (mainw->play_window!=NULL&&old_file==-1) {
      // usually preview or load_preview_frame would do this
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }
    // temp kludge, symlink audiodump.pcm to wav file, then pretend we are playing
    // an opening preview . Doesn't work with fifo.
    dummyvar=system ((tmp=g_strdup_printf ("/bin/ln -s %s/audiodump.pcm %s/%s/audiodump.pcm",prefs->tmpdir,prefs->tmpdir,cfile->handle)));
    g_free(tmp);
    play_file();
    mainw->noswitch=FALSE;
  }
  // TODO - else...
  
  if (mainw->current_file!=old_file&&mainw->current_file!=new_file) old_file=mainw->current_file; // we could have rendered to a new file

  mainw->current_file=new_file;

  close_current_file(old_file);

}


///////////////////////////////////////////////////////////////////////////////
// write functions - not used currently


gboolean 
lives_yuv_stream_start_write (lives_yuv4m_t * yuv4mpeg, const gchar *filename, gint hsize, gint vsize, gdouble fps) {
  int i;

  if (mainw->fixed_fpsd>-1.&&mainw->fixed_fpsd!=fps) {
    do_error_dialog(g_strdup_printf(_("Unable to set display framerate to %.3f fps.\n\n"),fps));
    return FALSE;
  }
  mainw->fixed_fpsd=fps;

  if (filename==NULL) filename=g_strdup_printf ("%s/streamout.yuv",prefs->tmpdir);

  // TODO - do_threaded_dialog
  if (!(yuvout=creat (filename,O_CREAT))) {
    do_error_dialog (g_strdup_printf (_("Unable to open yuv4mpeg out stream %s\n"),filename));
    return FALSE;
  }

  if (mainw->fixed_fpsd>23.9999&&mainw->fixed_fpsd<24.0001) {
    y4m_si_set_framerate(&(yuv4mpeg->streaminfo),y4m_fps_FILM);
  }
  else return FALSE;
  y4m_si_set_interlace(&(yuv4mpeg->streaminfo), Y4M_ILACE_NONE);

  y4m_si_set_width(&(yuv4mpeg->streaminfo), (hsize_out=hsize));
  y4m_si_set_height(&(yuv4mpeg->streaminfo), (vsize_out=vsize));
  y4m_si_set_sampleaspect(&(yuv4mpeg->streaminfo), yuv4mpeg->sar);
    
  i = y4m_write_stream_header(yuvout, &(yuv4mpeg->streaminfo));

  if (i != Y4M_OK) return FALSE;

  return TRUE;
}


gboolean 
lives_yuv_stream_write_frame (lives_yuv4m_t *yuv4mpeg, void *pixel_data) {
  // pixel_data is planar yuv420 data
  int i;

  guchar *planes[3];
  uint8_t *pixels=(guchar *)pixel_data;

  planes[0]=&(pixels[0]);
  planes[1]=&(pixels[hsize_out*vsize_out]);
  planes[2]=&(pixels[hsize_out*vsize_out*5/4]);

  i = y4m_write_frame(yuvout, &(yuv4mpeg->streaminfo),
  		&(yuv4mpeg->frameinfo), (uint8_t **)&planes[0]);
  if (i != Y4M_OK) return FALSE;
  return TRUE;
}


void 
lives_yuv_stream_stop_write (lives_yuv4m_t *yuv4mpeg) {
  y4m_fini_stream_info(&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info(&(yuv4mpeg->frameinfo));
  close (yuvout);
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





void
on_live_tvcard_activate                      (GtkMenuItem     *menuitem,
					      gpointer         user_data)
{
  gint cardno=0;

  gint new_file=mainw->first_free_file;

  gint response;

  gchar *com,*tmp;
  gchar *fifofile=g_strdup_printf("%s/tvpic.%d",prefs->tmpdir,getpid());

  gchar *chanstr;
  gchar *devstr;

  gchar *fname;

  GtkWidget *card_dialog;

  tvcardw_t *tvcardw;

  mainw->open_deint=FALSE;

  card_dialog=create_cdtrack_dialog(4,NULL);

  tvcardw=g_object_get_data(G_OBJECT(card_dialog),"tvcard_data");


  response=gtk_dialog_run(GTK_DIALOG(card_dialog));
  if (response==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(card_dialog);
    g_free(fifofile);
    g_free(tvcardw);
    return;
  }

  cardno=(gint)mainw->fx1_val;
  chanstr=g_strdup_printf("%d",(gint)mainw->fx2_val);

  if (g_list_find(mainw->videodevs,GINT_TO_POINTER(cardno))) {
    gtk_widget_destroy(card_dialog);
    do_card_in_use_error();
    g_free(chanstr);
    g_free(fifofile);
    g_free(tvcardw);
    return;
  }

  fname=g_strdup_printf(_("TV card %d"),cardno);

  if (!get_new_handle(new_file,fname)) {
    gtk_widget_destroy(card_dialog);
    g_free(chanstr);
    g_free(fifofile);
    g_free(fname);
    g_free(tvcardw);
    return;
  }

  devstr=g_strdup_printf("/dev/video%d",cardno);

  if (!check_dev_busy(devstr)) {
    gtk_widget_destroy(card_dialog);
    do_dev_busy_error(fname);
    g_free(devstr);
    g_free(chanstr);
    g_free(fifofile);
    g_free(fname);
    g_free(tvcardw);
    return;
  }

  mainw->videodevs=g_list_append(mainw->videodevs,GINT_TO_POINTER(cardno));

  mainw->current_file=new_file;

  cfile->deinterlace=mainw->open_deint;

  unlink(fifofile);
  mkfifo(fifofile,S_IRUSR|S_IWUSR);

  if (!tvcardw->use_advanced) {
    com=g_strdup_printf("smogrify open_tv_card %s \"%s\" %s %s",cfile->handle,chanstr,devstr,fifofile);
  }
  else {
    gdouble fps=0.;
    gchar *driver=NULL,*outfmt=NULL;
    gint width=0,height=0;
    gint input=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tvcardw->spinbuttoni));
    
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(tvcardw->radiobuttond))) {
      width=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tvcardw->spinbuttonw));
      height=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(tvcardw->spinbuttonh));
      fps=gtk_spin_button_get_value(GTK_SPIN_BUTTON(tvcardw->spinbuttonf));
    }

    driver=g_strdup(gtk_entry_get_text(GTK_ENTRY((GTK_COMBO(tvcardw->combod))->entry)));
    outfmt=g_strdup(gtk_entry_get_text(GTK_ENTRY((GTK_COMBO(tvcardw->comboo))->entry)));

    com=g_strdup_printf("smogrify open_tv_card %s \"%s\" %s %s %d %d %d %.3f %s %s",cfile->handle,chanstr,devstr,fifofile,input,width,height,fps,driver,outfmt);
    g_free(driver);
    g_free(outfmt);

  }
  gtk_widget_destroy(card_dialog);
  g_free(tvcardw);

  dummyvar=system(com);
  g_free(com);

  if (!open_yuv4m_inner(fifofile,fname,new_file,YUV4_TYPE_TV,cardno)) {
    g_free(fname);
    g_free(chanstr);
    g_free(fifofile);
    g_free(devstr);
    return;
  }
			       
  g_snprintf(cfile->type,40,"%s",fname);
  
  d_print ((tmp=g_strdup_printf (_("Opened TV card %d (%s)"),cardno,devstr)));

  g_free(tmp);
  g_free(fname);
  g_free(chanstr);
  g_free(devstr);
  g_free(fifofile);
    
}



void
on_live_fw_activate                      (GtkMenuItem     *menuitem,
					  gpointer         user_data)
{

  gchar *com,*tmp;
  gint cardno;
  gint cache=1024;

  gint new_file=mainw->first_free_file;

  gint response;

  gchar *fifofile=g_strdup_printf("%s/firew.%d",prefs->tmpdir,getpid());
  gchar *fname;

  GtkWidget *card_dialog;

  mainw->open_deint=FALSE;

  card_dialog=create_cdtrack_dialog(5,NULL);
  response=gtk_dialog_run(GTK_DIALOG(card_dialog));
  if (response==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(card_dialog);
    g_free(fifofile);
    return;
  }

  cardno=(gint)mainw->fx1_val;

  gtk_widget_destroy(card_dialog);

  if (g_list_find(fw_cards,GINT_TO_POINTER(cardno))) {
    g_free(fifofile);
    do_card_in_use_error();
    return;
  }

  fname=g_strdup_printf(_("Firewire card %d"),cardno);

  if (!get_new_handle(new_file,fname)) {
    g_free(fifofile);
    g_free(fname);
    return;
  }

  fw_cards=g_list_append(fw_cards,GINT_TO_POINTER(cardno));

  mainw->current_file=new_file;
  cfile->deinterlace=mainw->open_deint;

  unlink(fifofile);
  mkfifo(fifofile,S_IRUSR|S_IWUSR);

  com=g_strdup_printf("smogrify open_fw_card %s %d %d %s",cfile->handle,cardno,cache,fifofile);
  dummyvar=system(com);
  g_free(com);

  if (!open_yuv4m_inner(fifofile,fname,new_file,YUV4_TYPE_FW,cardno)) {
    g_free(fname);
    g_free(fifofile);
    return;
  }
			       
  g_snprintf(cfile->type,40,"%s",fname);
  
  d_print ((tmp=g_strdup_printf (_("Opened firewire card %d"),cardno)));

  g_free(tmp);
  g_free(fname);
  g_free(fifofile);

}
