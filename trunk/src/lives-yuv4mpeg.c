// yuv4mpeg.c
// LiVES
// (c) G. Finch 2004 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-effects.h"

#include "main.h"

#include "support.h"
#include "lives-yuv4mpeg.h"
#include <sys/types.h>
#include <errno.h>

static int yuvout,hsize_out,vsize_out;

lives_yuv4m_t *lives_yuv4mpeg_alloc (void)
{
  lives_yuv4m_t *yuv4mpeg = (lives_yuv4m_t *) malloc (sizeof(lives_yuv4m_t));
  if(!yuv4mpeg) return NULL;
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  y4m_init_stream_info (&(yuv4mpeg->streaminfo));
  y4m_init_frame_info (&(yuv4mpeg->frameinfo));
  return yuv4mpeg;
}



gboolean lives_yuv_stream_start_read (lives_yuv4m_t * yuv4mpeg, gchar *filename) {
  int i;

  if (filename==NULL) filename=g_strdup_printf ("%s/stream.yuv",prefs->tmpdir);

  // TODO - do_threaded_dialog
  if (!(yuv4mpeg->fd=open (filename,O_RDONLY))) {
    do_error_dialog (g_strdup_printf (_("Unable to open yuv4mpeg stream %s\n"),filename));
    return FALSE;
  }

  i = y4m_read_stream_header (yuv4mpeg->fd, &(yuv4mpeg->streaminfo));

  if (i != Y4M_OK) {
    gchar *tmp;
    d_print ((tmp=g_strdup_printf ("yuv4mpeg: %s\n", y4m_strerr (i))));
    g_free(tmp);
    return FALSE;
  }

  cfile->hsize = yuv4mpeg->hsize = y4m_si_get_width (&(yuv4mpeg->streaminfo));
  cfile->vsize = yuv4mpeg->vsize = y4m_si_get_height (&(yuv4mpeg->streaminfo));
  cfile->bpp = 12;

 //try to match the clip fps to input fps as near as possible
  cfile->fps=cfile->pb_fps=g_strtod (g_strdup_printf ("%.16f",Y4M_RATIO_DBL (y4m_si_get_framerate (&(yuv4mpeg->streaminfo)))),NULL);

  if(!(cfile->hsize*cfile->vsize))
    {
      do_error_dialog (g_strdup_printf (_("Video dimensions: %d x %d are invalid. Stream cannot be opened"),cfile->hsize,cfile->vsize));
      return FALSE;
    }
  return TRUE;
}


void lives_yuv_stream_stop_read (lives_yuv4m_t *yuv4mpeg) {
  y4m_fini_stream_info (&(yuv4mpeg->streaminfo));
  y4m_fini_frame_info (&(yuv4mpeg->frameinfo));
  yuv4mpeg->sar = y4m_sar_UNKNOWN;
  yuv4mpeg->dar = y4m_dar_4_3;
  close (yuv4mpeg->fd);
}


void weed_layer_set_from_yuv4m (weed_plant_t *layer, void *src) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)src;
  int i,error;
  void **pixel_data;

  weed_set_int_value(layer,"width",y4m_si_get_width (&(yuv4mpeg->streaminfo)));
  weed_set_int_value(layer,"height",y4m_si_get_height (&(yuv4mpeg->streaminfo)));
  weed_set_int_value(layer,"current_palette",WEED_PALETTE_YUV420P);

  create_empty_pixel_data(layer);

  pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  // TODO - this blocks, need to run it as a thread
  i = y4m_read_frame (yuv4mpeg->fd, &(yuv4mpeg->streaminfo),&(yuv4mpeg->frameinfo), (uint8_t **)pixel_data);
  
  if (i != Y4M_OK) {
    weed_free(pixel_data);
    return;
  }

  weed_free(pixel_data);

  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  return;
}





void
on_open_yuv4m_activate (GtkMenuItem *menuitem, gpointer user_data) {
  // start "playing" but open frames in yuv4mpeg format on stdin

  gint old_file=mainw->current_file,new_file;
  lives_yuv4m_t *yuv4mpeg;
  gchar *tmp;

  if (!do_yuv4m_open_warning()) return;

  // create a virtual clip
  new_file=mainw->first_free_file;
  if (!get_new_handle(new_file,"yuv4mpeg stream")) {
    mainw->error=TRUE;
    return;
  }

  mainw->current_file=new_file;
  cfile->clip_type=CLIP_TYPE_YUV4MPEG;

  // get size of frames, arate, achans, asamps, signed endian
  yuv4mpeg=lives_yuv4mpeg_alloc();
  if (!lives_yuv_stream_start_read(yuv4mpeg,user_data)) {
    close_current_file(old_file);
    return;
  }

  if (mainw->fixed_fpsd>0.&&(cfile->fps!=mainw->fixed_fpsd)) {
    do_error_dialog (_ ("\n\nUnable to open stream, framerate does not match fixed rate.\n"));
    close_current_file(old_file);
    return;
  }

  cfile->ext_src=yuv4mpeg;

  switch_to_file((mainw->current_file=old_file),new_file);
  set_main_title(cfile->file_name,0);
  add_to_winmenu();

  cfile->achans=0;
  cfile->asampsize=0;

  // open as a clip with 1 frame
  cfile->start=cfile->end=cfile->frames=1;
  cfile->arps=cfile->arate=0;
  mainw->fixed_fpsd=cfile->pb_fps=cfile->fps;

  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  cfile->changed=FALSE;

  // allow clip switching
  cfile->is_loaded=TRUE;

  g_snprintf(cfile->type,40,"yu4mpeg stream in");
  d_print ((tmp=g_strdup_printf (_("Opened yuv4mpeg stream on %sstream.yuv"),prefs->tmpdir)));
  g_free(tmp);
  d_print ((tmp=g_strdup_printf(_ (" size=%dx%d bpp=%d fps=%.3f\nAudio: "),cfile->hsize,yuv4mpeg->vsize,cfile->bpp,cfile->fps)));
  g_free(tmp);

  if (cfile->achans==0) {
    d_print (_ ("none\n"));
  }
  else {
    d_print ((tmp=g_strdup_printf(_ ("%d Hz %d channel(s) %d bps\n"),cfile->arate,cfile->achans,cfile->asampsize)));
    g_free(tmp);
  }

  d_print ((tmp=g_strdup_printf (_("Syncing to external framerate of %.8f frames per second.\n"),mainw->fixed_fpsd)));
  g_free(tmp);

  // if not playing, start playing
  if (mainw->playing_file==-1) {
    if (mainw->play_window!=NULL&&old_file==-1) {
      // usually preview or load_preview_frame would do this
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }
    mainw->play_start=1;
    mainw->play_end=INT_MAX;
    // temp kludge, symlink audiodump.pcm to wav file, then pretend we are playing
    // an opening preview . Doesn't work with fifo.
    system ((tmp=g_strdup_printf ("/bin/ln -s %s/audiodump.pcm %s/%s/audiodump.pcm",prefs->tmpdir,prefs->tmpdir,cfile->handle)));
    g_free(tmp);
    play_file();
    mainw->noswitch=FALSE;
  }
  // TODO - else...
  
  if (mainw->current_file!=old_file&&mainw->current_file!=new_file) old_file=mainw->current_file; // we could have rendered to a new file

  mainw->fixed_fpsd=-1.;
  d_print (_("Sync lock off.\n"));
  mainw->current_file=new_file;
  lives_yuv_stream_stop_read (yuv4mpeg);
  g_free (cfile->ext_src);
  cfile->ext_src=NULL;

  close_current_file(old_file);

}



gboolean 
lives_yuv_stream_start_write (lives_yuv4m_t * yuv4mpeg, gchar *filename, gint hsize, gint vsize, gdouble fps) {
  int i;
  // currently unused - TODO - remove


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
