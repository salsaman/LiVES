// LiVES - videodev input
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

//#define TEST_V4L_IN
#ifdef TEST_V4L_IN

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>

#include "support.h"
#include "main.h"




/// grab frames during playback


void weed_layer_set_from_lvdev (weed_plant_t *layer, file *sfile) {
  lives_yuv4m_t *yuv4mpeg=(lives_yuv4m_t *)(sfile->ext_src);
  int error;

  pthread_t y4thread;
  struct timeval otv;
  int64_t ntime=0,stime;

  weed_set_int_value(layer,"width",sfile->hsize);
  weed_set_int_value(layer,"height",sfile->vsize);
  weed_set_int_value(layer,"current_palette",WEED_PALETTE_YUV420P);
  weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);

  create_empty_pixel_data(layer);

  if((*(ldev->c->actions->start_capture))(ldev->vdev)!=0){
    (ldev->c->actions->free_capture)(v);
    free_capture_device(ldev->vdev);
    close_device(ldev->vdev);
    printf("Cant start capture");
    return -1;
  }

  if((ptr = (*ldev->c->actions->dequeue_buffer)(ldev->vdev, &size)) != NULL) {
    //do something useful with frame at 'd' of size 'size'

    copy_planar_to_pixel_data(d,pixel_data,ldev->current_palette,sfile->hsize,sfile->vsize);

    //when finished put the buffer back
    //Put buffer
    (*ldev->c->actions->enqueue_buffer)(ldev->vdev);
  } else {
    g_printerr("Cant get buffer ");
    break;
  }

  ret = ldev->cap->actions->stop_capture(ldev->vdev);

  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  return;
}






/// get devnumber from user and open it to a new clip

typedef struct {
  struct video_device *vdev;
  struct device_info *i
} lives_vdev_t;

static gboolean open_vdev_inner(int new_file, int devno, int chan) {
  // create a virtual clip
  int nbuffs=4;

  int old_file=mainw->current_file;
  int j,k;
  int std=0;

  int fmts[]={RGB24;BGR24;RGB32;BGR32;YUV32;UYVY;VYUY;YUV422P;YUV420;YVU420};
  int fmts_nb=10, ret;

  lives_ldev_t *ldev=(lives_ldev_t *)g_malloc(sizeof(lives_ldev_t));

  cfile->clip_type=CLIP_TYPE_VIDEODEV;

  // open dev
  lvdev->vdev = open_device(filename);

  //check return value and take appropriate action
  if (lvdev->vdev==NULL) {
    fprintf (stderr, "vdev input: cannot open %s %s\n",filename);
    g_free(ldev);
    return FALSE;
  }

  // get details

  // std can be WEBCAM, PAL, SECAM or NTSC

  lvdev->c = init_capture_device(lvdev->vdev, DEF_GEN_WIDTH, DEF_GEN_HEIGHT , chan, std, nbuffs);
  if(lvdev->c==NULL) {
    fprintf(stderr,"Error initialising device.\n");
    close_device(lvdev->vdev);
    return FALSE;
  }
  
  ret = ldev->c->actions->set_cap_param(ldev->vdev, fmts, fmts_nb);
  if(ret!=0) {
    fprintf(stderr,"Error setting caps.\n");
    close_device(lvdev->vdev);
    return FALSE;
  }

  cfile->hsize=ldev->c->width;
  cfile->vsize=ldev->c->height;

  ldev->current_palette=libvidp_to_weedp(c->palette);

  ret = ldev->c->actions->get_frame_interval(ldev->vdev, &num, &denom);
  if(ret!=0) {
    fprintf(stderr,"Error getting fps.\n");
    close_device(lvdev->vdev);
    return FALSE;
  }

  if (num!=1) {
    cfile->ratio_fps=TRUE;
    cfile->fps=(gdouble)denom/(gdouble)num;
    fps_string=g_strdup_printf("%.8f",fps);
    cfile->fps=g_strtod(fps_string,NULL);
    g_free(fps_string);
  }
  else cfile->fps=(gdouble)denom;

  cfile->ext_src=lvdev;

  cfile->bpp = weed_palette_get_compression_ratio(ldev->current_palette) * 
    weed_palette_has_alpha_channel(ldev->current_palette)?32:24;

  cfile->start=cfile->end=cfile->frames=1;

  cfile->is_loaded=TRUE;

  add_to_winmenu();

  switch_to_file((mainw->current_file=old_file),new_file);

  return TRUE;
}









void
on_openvdev_activate                      (GtkMenuItem     *menuitem,
					   gpointer         user_data)
{

  gint devno=0,chan;

  gint new_file=mainw->first_free_file;
  gint old_file=mainw->current_file;

  gint response;

  gchar *tmp;
  gchar *fname;

  GtkWidget *card_dialog;

  mainw->open_deint=FALSE;

  card_dialog=create_cdtrack_dialog(4,NULL);
  response=gtk_dialog_run(GTK_DIALOG(card_dialog));
  if (response==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(card_dialog);
    return;
  }

  devno=(gint)mainw->fx1_val;
  chan=(gint)mainw->fx2_val;

  gtk_widget_destroy(card_dialog);

  if (g_list_find(tv_cards,GINT_TO_POINTER(devno))) {
    do_card_in_use_error();
    return;
  }

  fname=g_strdup_printf(_("/dev/video%d"),devno);

  if (!check_dev_busy(fname)) {
    do_dev_busy_error(fname);
    g_free(fname);
    return;
  }

  if (!get_new_handle(new_file,fname)) {
    g_free(fname);
    return;
  }

  tv_cards=g_list_append(tv_cards,GINT_TO_POINTER(devno));

  mainw->current_file=new_file;

  cfile->deinterlace=mainw->open_deint;

  if (!open_vdev_inner(new_file,devno,chan)) {
    g_free(fname);
    close_current_file(old_file);
    return;
  }
			       
  g_snprintf(cfile->type,40,"%s",fname);
  
  d_print ((tmp=g_strdup_printf (_("Opened /dev/video%d"),devno)));

  g_free(tmp);
  g_free(fname);
}



#endif
