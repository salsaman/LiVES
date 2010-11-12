// LiVES - videodev input
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef TEST_UNICAP

#include "support.h"
#include "main.h"
#include "videodev.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-host.h"

#include <unicap/unicap.h>

typedef struct {
  unicap_handle_t handle;
  unicap_data_buffer_t buffer;
  int current_palette;
} lives_vdev_t;


#define MAX_DEVICES 1024
#define MAX_FORMATS 1024

////////////////////////////////////////////////////


gboolean weed_layer_set_from_lvdev (weed_plant_t *layer, file *sfile) {
  lives_vdev_t *ldev=sfile->ext_src;
  unicap_data_buffer_t *returned_buffer;
  void **pixel_data;
  int error,rowstride;
  
  weed_set_int_value(layer,"width",sfile->hsize);
  weed_set_int_value(layer,"height",sfile->vsize);
  weed_set_int_value(layer,"current_palette",ldev->current_palette);
  weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);
  weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);

  create_empty_pixel_data(layer);
  unicap_start_capture (ldev->handle); 

  // for planar palettes we can use pixel_data directly
  if (weed_palette_get_numplanes(ldev->current_palette)==1) {
    rowstride=weed_get_int_value(layer,"rowstrides",&error);
    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
    ldev->buffer.data=pixel_data[0];
    ldev->buffer.buffer_size=rowstride*sfile->vsize;
    weed_free(pixel_data);
  }

  // TODO - we should use 2 buffers to prevent long waits here
  unicap_queue_buffer (ldev->handle, &ldev->buffer);
  if (!SUCCESS (unicap_wait_buffer (ldev->handle, &returned_buffer))||returned_buffer!=&ldev->buffer) {
    if (returned_buffer!=&ldev->buffer) {
      // should never happen
      free(returned_buffer->data);
      free(returned_buffer);
    }
    g_printerr("Failed to wait for buffer!\n");
    return FALSE;
  }

  if (weed_palette_get_numplanes(ldev->current_palette)>1) {
    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
    pixel_data_planar_from_membuf(pixel_data, returned_buffer->data, sfile->hsize*sfile->vsize, ldev->current_palette);
    if (returned_buffer!=&ldev->buffer) {
      // should never happen
      free(returned_buffer->data);
      free(returned_buffer);
    }
  }

  // TODO - keep capture going until we have a new frame ready
  unicap_stop_capture (ldev->handle);

  return TRUE;
}


static unicap_format_t *lvdev_get_best_format(const unicap_format_t *formats, 
						    lives_vdev_t *ldev, int palette, int width, int height) {
  // get nearest format for given palette, width and height
  // if palette is WEED_PALETTE_END, or cannot be matched, get best quality palette (preferring RGB)
  // width and height must be set, actual width and height will be set as near to this as possible
  // giving preference to larger frame size

  // if the device supports no usable formats, returns NULL

  // Note: we match first by palette, then by size

  int format_count,i;
  unicap_status_t status = STATUS_SUCCESS;
  int f=-1;
  int bestp=WEED_PALETTE_END;
  int bestw=0,besth=0;
  int cpal;

  // get details
  for (format_count = 0; SUCCESS (status) && (format_count < MAX_FORMATS);format_count++){
    unicap_format_t *format=(unicap_format_t *)(&formats[format_count]);
    status = unicap_enumerate_formats (ldev->handle, NULL, (unicap_format_t *)format, format_count);

    // TODO - check if we need to free format->sizes

    // TODO - prefer non-interlaced, YCbCr
    cpal=fourccp_to_weedp(format->fourcc,format->bpp,NULL,NULL);

    if (cpal==WEED_PALETTE_END||weed_palette_is_alpha_palette(cpal)) {
      g_printerr("Unusable palette with fourcc %xd",format->fourcc);
      continue;
    }

    if (bestp==WEED_PALETTE_END||cpal==palette||weed_palette_is_alpha_palette(bestp)||weed_palette_is_lower_quality(bestp,cpal)||(weed_palette_is_yuv_palette(bestp)&&weed_palette_is_rgb_palette(cpal))) {
      // got better palette, or exact match

      // prefer exact match on target palette if we have it
      if (palette!=WEED_PALETTE_END && bestp==palette && cpal!=palette) continue;

      // otherwise this is our best palette up to now
      bestp=cpal;

      // TODO - try to minimise aspect delta
      // for now we just go with the smallest size >= target (or largest frame size if none are >= target)

      if (width>=format->min_size.width && height>=format->min_size.height) {
	if (format->h_stepping>0&&format->v_stepping>0) {

	  // can set exact size (within stepping limits)
	  format->size.width=(int)(((double)width+(double)format->h_stepping/2.)
				   /(double)format->h_stepping) * format->h_stepping;

	  format->size.height=(int)(((double)height+(double)format->v_stepping/2.)
				   /(double)format->v_stepping) * format->v_stepping;

	  if (format->size.width>format->max_size.width) format->size.width=format->max_size.width;
	  if (format->size.height>format->max_size.height) format->size.height=format->max_size.height;

	  if (format->size.width>bestw || format->size.height>besth) {
	    bestw=format->size.width;
	    besth=format->size.height;
	    f=format_count;
	  }
	}
	else {
	  // array of sizes supported
	  // step through sizes
	  for (i=0;i<format->size_count;i++) {
	    if (format->sizes[i].width>bestw&&format->sizes[i].height>besth&&
		(i==format->size_count-1||bestw<width||besth<height)) {
	      // this format supports a better size match
	      bestw=format->size.width=format->sizes[i].width;
	      besth=format->size.height=format->sizes[i].height;
	      f=format_count;
	    }
	  }
	}
      }
      else {
	// target is smaller than min width, height
	if (bestw<format->min_size.width||besth<format->min_size.height) continue; // TODO - minimise aspect delta
	bestw=format->size.width=format->min_size.width;
	besth=format->size.height=format->min_size.height;
	f=format_count;
      }
    }
  }

  if (f>-1) return (unicap_format_t *)(&formats[f]);
  return NULL;
}





/// get devnumber from user and open it to a new clip

static gboolean open_vdev_inner(unicap_device_t *device) {
  // create a virtual clip
  int old_file=mainw->current_file;
  int new_file;
  lives_vdev_t *ldev=(lives_vdev_t *)g_malloc(sizeof(lives_vdev_t));
  const unicap_format_t formats[MAX_FORMATS];
  unicap_format_t *format;

  cfile->clip_type=CLIP_TYPE_VIDEODEV;

  // open dev
  unicap_open(&ldev->handle, device);

  //check return value and take appropriate action
  if (ldev->handle==NULL) {
    g_printerr ("vdev input: cannot open device\n");
    g_free(ldev);
    return FALSE;
  }

  format=lvdev_get_best_format(formats, ldev, WEED_PALETTE_END, DEF_GEN_WIDTH, DEF_GEN_HEIGHT);
  
  if(format==NULL) {
    g_printerr("No useful formats found.\n");
    unicap_close(ldev->handle);
    g_free(ldev);
    return FALSE;
  }

  format->buffer_type = UNICAP_BUFFER_TYPE_USER;

  // ignore YUV subspace for now
  ldev->current_palette=fourccp_to_weedp(format->fourcc, format->bpp, &cfile->interlace, NULL);

  if (!SUCCESS (unicap_set_format (ldev->handle, format))) {
    g_printerr("Error setting format.\n");
    unicap_close(ldev->handle);
    g_free(ldev);
    return FALSE;
  }

  cfile->hsize=format->size.width;
  cfile->vsize=format->size.height;

  cfile->ext_src=ldev;

  if (weed_palette_get_numplanes(ldev->current_palette)>1) {
    // TODO - needs freeing
    ldev->buffer.data = g_malloc (format->buffer_size);
    ldev->buffer.buffer_size = format->buffer_size;
  }

  cfile->bpp = format->bpp;

  cfile->start=cfile->end=cfile->frames=1;

  cfile->is_loaded=TRUE;

  add_to_winmenu();

  switch_to_file((mainw->current_file=old_file),new_file);

  return TRUE;
}







void on_openvdev_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gint devno=0;

  gint new_file=mainw->first_free_file;
  gint old_file=mainw->current_file;

  gint response;

  gchar *tmp;
  gchar *fname;
  gchar **devarray;

  GtkWidget *card_dialog;

  int i,dev_count;
  int status = STATUS_SUCCESS;
  unicap_device_t devices[MAX_DEVICES];

  mainw->open_deint=FALSE;

  // get device list
  for (dev_count = 0; SUCCESS (status) && (dev_count < MAX_DEVICES);
       dev_count++)
    {
      status = unicap_enumerate_devices (NULL, &devices[dev_count], dev_count);
      if (SUCCESS (status))
        printf ("%d: %s\n", dev_count, devices[dev_count].identifier);
      else
        break;
    }

  if (dev_count == 0) {
    do_no_in_devs_error();
    return;
  }

  devarray=g_malloc((dev_count+1)*sizeof(char *));
  for (i=0;i<dev_count;i++) {
    devarray[i]=devices[i].identifier; // consider strings as const
  }
  devarray[i]=NULL;

  mainw->fx1_val=0;
  card_dialog=create_combo_dialog(1,(gpointer)devarray);
  g_free(devarray);

  response=gtk_dialog_run(GTK_DIALOG(card_dialog));
  if (response==GTK_RESPONSE_CANCEL) {
    gtk_widget_destroy(card_dialog);
    return;
  }

  devno=(gint)mainw->fx1_val;

  gtk_widget_destroy(card_dialog);

  fname=g_strdup_printf("%s",devices[devno].device);

  if (!get_new_handle(new_file,fname)) {
    g_free(fname);
    return;
  }

  mainw->current_file=new_file;

  cfile->deinterlace=mainw->open_deint;

  if (!open_vdev_inner(&devices[devno])) {
    g_free(fname);
    close_current_file(old_file);
    return;
  }
			       
  g_snprintf(cfile->type,40,"%s",fname);

  d_print ((tmp=g_strdup_printf (_("Opened device %s"),devices[devno].identifier)));

  g_free(tmp);
  g_free(fname);
}



#endif

