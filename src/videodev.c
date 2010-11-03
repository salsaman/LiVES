// LiVES - videodev input
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

//#define TEST_V4L_IN
#ifdef TEST_V4L_IN



#include "support.h"
#include "main.h"
#include "../libweed/weed-palettes.h"
#include "../libvideo/libvideo.h"

/// grab frames during playback

typedef struct {
  struct video_device *vdev;
  struct device_info *i;
  struct capture_device *c;
  int current_palette;
} lives_vdev_t;


gboolean weed_layer_set_from_lvdev (weed_plant_t *layer, file *sfile) {
  int error,ret;
  void *ptr;
  lives_vdev_t *ldev=sfile->ext_src;
  int size;

  weed_set_int_value(layer,"width",sfile->hsize);
  weed_set_int_value(layer,"height",sfile->vsize);
  weed_set_int_value(layer,"current_palette",ldev->current_palette);
  weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);
  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  create_empty_pixel_data(layer);

  if ((*ldev->c->actions->start_capture)(ldev->vdev)!=0){
    (*ldev->c->actions->free_capture)(ldev->vdev);
    free_capture_device(ldev->vdev);
    close_device(ldev->vdev);
    g_printerr("Cant start capture");
    return FALSE;
  }

  if((ptr = (*ldev->c->actions->dequeue_buffer)(ldev->vdev, &size)) != NULL) {
    //do something useful with frame at 'd' of size 'size'
    void **pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
    copy_planar_to_pixel_data(ptr,pixel_data,ldev->current_palette,sfile->hsize,sfile->vsize);

    //when finished put the buffer back
    //Put buffer
    (*ldev->c->actions->enqueue_buffer)(ldev->vdev);
  } else {
    g_printerr("Cant get buffer ");
    (*ldev->c->actions->stop_capture)(ldev->vdev);
    return FALSE;
  }

  ret = (*ldev->c->actions->stop_capture)(ldev->vdev);

  return TRUE;
}






/// get devnumber from user and open it to a new clip

static gboolean open_vdev_inner(gchar *filename, int new_file, int devno, int chan) {
  // create a virtual clip
  int nbuffs=4;
  int num,denom;

  int old_file=mainw->current_file;
  int std=0;

  int fmts[]={RGB24,BGR24,RGB32,BGR32,YUV32,UYVY,VYUY,YUV422P,YUV420,YVU420};
  int fmts_nb=10, ret;

  lives_vdev_t *ldev=(lives_vdev_t *)g_malloc(sizeof(lives_vdev_t));

  cfile->clip_type=CLIP_TYPE_VIDEODEV;

  // open dev
  ldev->vdev = open_device(filename);

  //check return value and take appropriate action
  if (ldev->vdev==NULL) {
    g_printerr ("vdev input: cannot open %s\n",filename);
    g_free(ldev);
    return FALSE;
  }

  // get details

  // std can be WEBCAM, PAL, SECAM or NTSC

  ldev->c = init_capture_device(ldev->vdev, DEF_GEN_WIDTH, DEF_GEN_HEIGHT , chan, std, nbuffs);
  if(ldev->c==NULL) {
    fprintf(stderr,"Error initialising device.\n");
    close_device(ldev->vdev);
    return FALSE;
  }
  
  ret = ldev->c->actions->set_cap_param(ldev->vdev, fmts, fmts_nb);
  if(ret!=0) {
    fprintf(stderr,"Error setting caps.\n");
    close_device(ldev->vdev);
    return FALSE;
  }

  cfile->hsize=ldev->c->width;
  cfile->vsize=ldev->c->height;

  ldev->current_palette=libvidp_to_weedp(ldev->c->palette);

  ret = ldev->c->actions->get_frame_interval(ldev->vdev, &num, &denom);
  if(ret!=0) {
    fprintf(stderr,"Error getting fps.\n");
    close_device(ldev->vdev);
    return FALSE;
  }

  if (num!=1) {
    gchar *fps_string;
    cfile->ratio_fps=TRUE;
    cfile->fps=(gdouble)denom/(gdouble)num;
    fps_string=g_strdup_printf("%.8f",cfile->fps);
    cfile->fps=g_strtod(fps_string,NULL);
    g_free(fps_string);
  }
  else cfile->fps=(gdouble)denom;

  cfile->ext_src=ldev;

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

  if (g_list_find(mainw->videodevs,GINT_TO_POINTER(devno))) {
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

  mainw->videodevs=g_list_append(mainw->videodevs,GINT_TO_POINTER(devno));

  mainw->current_file=new_file;

  cfile->deinterlace=mainw->open_deint;

  if (!open_vdev_inner(fname,new_file,devno,chan)) {
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
