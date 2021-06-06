// LiVES - videodev input
// (c) G. Finch 2010 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "main.h"

#ifdef HAVE_UNICAP
#define DEBUG_UNICAP

#include "videodev.h"
#include "interface.h"
#include "callbacks.h"
#include "effects-weed.h"

#include <unicap/unicap.h>

static boolean lives_wait_user_buffer(lives_vdev_t *ldev, unicap_data_buffer_t **buff, double timeout) {
  // wait for USER type buffer
  unicap_status_t status;
  int ncount;
  lives_alarm_t alarm_handle = lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);

  do {
    status = unicap_poll_buffer(ldev->handle, &ncount);

#ifdef DEBUG_UNICAP
    if (status != STATUS_SUCCESS) lives_printerr("Unicap poll failed with status %d\n", status);
#endif
    if (ncount >= 0) {
      lives_alarm_clear(alarm_handle);
      if (!SUCCESS(unicap_wait_buffer(ldev->handle, buff))) return FALSE;
      return TRUE;
    }
    lives_usleep(prefs->sleep_time);
    lives_widget_context_update();
    sched_yield();
  } while (lives_alarm_check(alarm_handle) > 0);

  return FALSE;
}


static boolean lives_wait_system_buffer(lives_vdev_t *ldev, double timeout) {
  // wait for SYSTEM type buffer
  lives_alarm_t alarm_handle = lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);

  do {
    if (ldev->buffer_ready != 0) {
      lives_alarm_clear(alarm_handle);
      return TRUE;
    }
    lives_usleep(prefs->sleep_time);
    lives_widget_context_update();
    sched_yield();
  } while (lives_alarm_check(alarm_handle) > 0);
  lives_alarm_clear(alarm_handle);

  return FALSE;
}


static void new_frame_cb(unicap_event_t event, unicap_handle_t handle,
                         unicap_data_buffer_t *buffer, void *usr_data) {
  lives_vdev_t *ldev = (lives_vdev_t *)usr_data;
  if (!LIVES_IS_PLAYING || (mainw->playing_file != ldev->fileno && mainw->blend_file != ldev->fileno)) {
    ldev->buffer_ready = 0;
    return;
  }

  if (ldev->buffer_ready != 1) {
    lives_memcpy(ldev->buffer1.data, buffer->data, ldev->buffer1.buffer_size);
    ldev->buffer_ready = 1;
  } else {
    lives_memcpy(ldev->buffer2.data, buffer->data, ldev->buffer2.buffer_size);
    ldev->buffer_ready = 2;
  }
}


boolean weed_layer_set_from_lvdev(weed_layer_t *layer, lives_clip_t *sfile, double timeoutsecs) {
  lives_vdev_t *ldev = (lives_vdev_t *)sfile->ext_src;
  unicap_data_buffer_t *returned_buffer = NULL;
  void **pixel_data;
  void *odata = ldev->buffer1.data;

  int error;

  weed_set_int_value(layer, WEED_LEAF_WIDTH, sfile->hsize /
                     weed_palette_get_pixels_per_macropixel(ldev->current_palette));
  weed_set_int_value(layer, WEED_LEAF_HEIGHT, sfile->vsize);
  weed_set_int_value(layer, WEED_LEAF_CURRENT_PALETTE, ldev->current_palette);
  weed_set_int_value(layer, WEED_LEAF_YUV_SUBSPACE, WEED_YUV_SUBSPACE_YCBCR); // TODO - handle bt.709
  weed_set_int_value(layer, WEED_LEAF_YUV_SAMPLING, WEED_YUV_SAMPLING_DEFAULT); // TODO - use ldev->YUV_sampling
  weed_set_int_value(layer, WEED_LEAF_YUV_CLAMPING, ldev->YUV_clamping);

  create_empty_pixel_data(layer, TRUE, TRUE);

  if (ldev->buffer_type == UNICAP_BUFFER_TYPE_USER) {
    if (weed_palette_get_nplanes(ldev->current_palette) == 1 || ldev->is_really_grey) {
      ldev->buffer1.data = (unsigned char *)weed_get_voidptr_value(layer, WEED_LEAF_PIXEL_DATA, &error);
    }

    unicap_queue_buffer(ldev->handle, &ldev->buffer1);

    if (!lives_wait_user_buffer(ldev, &returned_buffer, timeoutsecs)) {
#ifdef DEBUG_UNICAP
      lives_printerr("Failed to wait for user buffer!\n");
      unicap_stop_capture(ldev->handle);
      unicap_dequeue_buffer(ldev->handle, &returned_buffer);
      unicap_start_capture(ldev->handle);
#endif
      ldev->buffer1.data = (unsigned char *)odata;
      return FALSE;
    }
  } else {
    // wait for callback to fill buffer
    if (!lives_wait_system_buffer(ldev, timeoutsecs)) {
#ifdef DEBUG_UNICAP
      lives_printerr("Failed to wait for system buffer!\n");
#endif
    }
    if (ldev->buffer_ready == 1) returned_buffer = &ldev->buffer1;
    else returned_buffer = &ldev->buffer2;
  }

  pixel_data = weed_get_voidptr_array(layer, WEED_LEAF_PIXEL_DATA, &error);

  if (weed_palette_get_nplanes(ldev->current_palette) > 1 && !ldev->is_really_grey) {
    boolean contig = FALSE;
    if (weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, &error) == WEED_TRUE) contig = TRUE;
    pixel_data_planar_from_membuf(pixel_data, returned_buffer->data, sfile->hsize * sfile->vsize, ldev->current_palette, contig);
  } else {
    if (ldev->buffer_type == UNICAP_BUFFER_TYPE_SYSTEM) {
      int rowstride = weed_get_int_value(layer, WEED_LEAF_ROWSTRIDES, &error);
      size_t bsize = rowstride * sfile->vsize;
      if (bsize > returned_buffer->buffer_size) {
#ifdef DEBUG_UNICAP
        lives_printerr("Warning - returned buffer size too small !\n");
#endif
        bsize = returned_buffer->buffer_size;
      }
      lives_memcpy(pixel_data[0], returned_buffer->data, bsize);
    }
  }

  // shouldn't be necessary since we specified black_fill in create_empty_pixel_data()

  /* if (ldev->is_really_grey) { */
  /*   // y contains our greyscale data */
  /*   // set u and v planes to 128 */
  /*   memset(pixel_data[1], 128, sfile->hsize * sfile->vsize); */
  /*   memset(pixel_data[2], 128, sfile->hsize * sfile->vsize); */
  /* } */

  lives_free(pixel_data);

  ldev->buffer1.data = (unsigned char *)odata;

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

  int format_count, i;
  unicap_format_t *format;
  int f = -1;
  int bestp = WEED_PALETTE_END;
  int bestw = 0, besth = 0;
  int cpal;

  // get details
  for (format_count = 0;
       SUCCESS(unicap_enumerate_formats(ldev->handle, NULL, (unicap_format_t *)&formats[format_count], format_count))
       && (format_count < MAX_FORMATS); format_count++) {
    format = (unicap_format_t *)&formats[format_count];

    // TODO - check if we need to free format->sizes

    // TODO - prefer non-interlaced, YCbCr for YUV
    cpal = fourccp_to_weedp(format->fourcc, format->bpp, NULL, NULL, NULL, NULL);

    if (cpal == WEED_PALETTE_END || weed_palette_is_alpha(cpal)) {
#ifdef DEBUG_UNICAP
      // set format to try and get more data
      unicap_set_format(ldev->handle, format);
      lives_printerr("Unusable palette with fourcc 0x%x  bpp=%d, size=%dx%d buf=%d\n", format->fourcc, format->bpp,
                     format->size.width,
                     format->size.height, (int)format->buffer_size);
#endif
      continue;
    }

    if (bestp == WEED_PALETTE_END || cpal == palette || weed_palette_is_alpha(bestp) ||
        weed_palette_is_lower_quality(bestp, cpal) ||
        (weed_palette_is_yuv(bestp) && weed_palette_is_rgb(cpal))) {
      // got better palette, or exact match

      // prefer exact match on target palette if we have it
      if (palette != WEED_PALETTE_END && bestp == palette && cpal != palette) continue;

      // otherwise this is our best palette up to now
      bestp = cpal;

      // TODO - try to minimise aspect delta
      // for now we just go with the smallest size >= target (or largest frame size if none are >= target)

      if (width >= format->min_size.width && height >= format->min_size.height) {
        if (format->h_stepping > 0 && format->v_stepping > 0) {
#ifdef DEBUG_UNICAP
          lives_printerr("Can set any size with step %d and %d; min %d x %d, max %d x %d\n",
                         format->h_stepping, format->v_stepping,
                         format->min_size.width, format->min_size.height, format->max_size.width, format->max_size.height);
#endif
          // can set exact size (within stepping limits)
          format->size.width = (int)(((double)width + (double)format->h_stepping / 2.)
                                     / (double)format->h_stepping) * format->h_stepping;

          format->size.height = (int)(((double)height + (double)format->v_stepping / 2.)
                                      / (double)format->v_stepping) * format->v_stepping;

          if (format->size.width > format->max_size.width) format->size.width = format->max_size.width;
          if (format->size.height > format->max_size.height) format->size.height = format->max_size.height;

          if (format->size.width > bestw || format->size.height > besth) {
            bestw = format->size.width;
            besth = format->size.height;
            f = format_count;
          }
        } else {
          // array of sizes supported
          // step through sizes
#ifdef DEBUG_UNICAP
          lives_printerr("Checking %d array sizes\n", format->size_count);
#endif

          if (format->size_count == 0) {
            // only one size we can use, this is it...

            if ((format->size.width > bestw || format->size.height > besth) && (bestw < width || besth < height)) {
              // this format supports a better size match
              bestw = format->size.width;
              besth = format->size.height;
              f = format_count;
#ifdef DEBUG_UNICAP
              lives_printerr("Size is best so far\n");
#endif
            }
            continue;
          }

          // array of sizes
          for (i = 0; i < format->size_count; i++) {
#ifdef DEBUG_UNICAP
            lives_printerr("entry %d:%d x %d\n", i, format->sizes[i].width, format->sizes[i].height);
#endif
            if (format->sizes[i].width > bestw && format->sizes[i].height > besth &&
                (bestw < width || besth < height)) {
              // this format supports a better size match
              bestw = format->size.width = format->sizes[i].width;
              besth = format->size.height = format->sizes[i].height;
              f = format_count;
#ifdef DEBUG_UNICAP
              lives_printerr("Size is best so far\n");
#endif
            }
          }
        }
      } else {
        // target is smaller than min width, height
        if (bestw < format->min_size.width || besth < format->min_size.height) continue; // TODO - minimise aspect delta
        bestw = format->size.width = format->min_size.width;
        besth = format->size.height = format->min_size.height;
        f = format_count;
      }
    }
  }

  if (f > -1) return (unicap_format_t *)(&formats[f]);
  return NULL;
}


/// get devnumber from user and open it to a new clip

static boolean open_vdev_inner(unicap_device_t *device) {
  // create a virtual clip
  lives_vdev_t *ldev = (lives_vdev_t *)lives_malloc(sizeof(lives_vdev_t));
  unicap_format_t formats[MAX_FORMATS];
  unicap_format_t *format;

  // open dev
  unicap_open(&ldev->handle, device);

  //check return value and take appropriate action
  if (!ldev->handle) {
    LIVES_ERROR("vdev input: cannot open device");
    lives_free(ldev);
    return FALSE;
  }

  unicap_lock_stream(ldev->handle);

  format = lvdev_get_best_format(formats, ldev, WEED_PALETTE_END, DEF_GEN_WIDTH, DEF_GEN_HEIGHT);

  if (!format) {
    LIVES_INFO("No useful formats found");
    unicap_unlock_stream(ldev->handle);
    unicap_close(ldev->handle);
    lives_free(ldev);
    return FALSE;
  }

  if (!(format->buffer_types & UNICAP_BUFFER_TYPE_USER)) {
    // have to use system buffer type
    format->buffer_type = UNICAP_BUFFER_TYPE_SYSTEM;

    // set a callback for new frame
    unicap_register_callback(ldev->handle, UNICAP_EVENT_NEW_FRAME, (unicap_callback_t) new_frame_cb,
                             (void *) ldev);

  } else format->buffer_type = UNICAP_BUFFER_TYPE_USER;

  ldev->buffer_type = format->buffer_type;

  // ignore YUV subspace for now
  ldev->current_palette = fourccp_to_weedp(format->fourcc, format->bpp, (int *)&cfile->interlace,
                          &ldev->YUV_sampling, &ldev->YUV_subspace, &ldev->YUV_clamping);

#ifdef DEBUG_UNICAP
  lives_printerr("\nUsing palette with fourcc 0x%x, translated as %s\n", format->fourcc,
                 weed_palette_get_name(ldev->current_palette));
#endif

  if (!SUCCESS(unicap_set_format(ldev->handle, format))) {
    LIVES_ERROR("Unicap error setting format");
    unicap_unlock_stream(ldev->handle);
    unicap_close(ldev->handle);
    lives_free(ldev);
    return FALSE;
  }

  g_print("ALLX %ld %d %d %d %d\n", format->buffer_size, format->size.width, format->size.height,
          weed_palette_get_bits_per_macropixel(
            ldev->current_palette), weed_palette_get_pixels_per_macropixel(ldev->current_palette));

  if (format->buffer_size != format->size.width * format->size.height * weed_palette_get_bits_per_macropixel(
        ldev->current_palette) /
      weed_palette_get_pixels_per_macropixel(ldev->current_palette) / 8) {
    int wwidth = format->size.width, awidth;
    int wheight = format->size.height, aheight;
    // something went wrong setting the size - the buffer is wrongly sized
#ifdef DEBUG_UNICAP
    lives_printerr("Unicap buffer size is wrong, resetting it.\n");
#endif
    // get the size again

    unicap_get_format(ldev->handle, format);
    awidth = format->size.width;
    aheight = format->size.height;

#ifdef DEBUG_UNICAP
    lives_printerr("Wanted frame size %d x %d, got %d x %d\n", wwidth, wheight, awidth, aheight);
#endif

    format->buffer_size = format->size.width * format->size.height * weed_palette_get_bits_per_macropixel(ldev->current_palette) /
                          weed_palette_get_pixels_per_macropixel(ldev->current_palette) / 8;
  }

  cfile->hsize = format->size.width;
  cfile->vsize = format->size.height;

  cfile->ext_src = ldev;
  cfile->ext_src_type = LIVES_EXT_SRC_DEVICE;

  ldev->buffer1.data = (unsigned char *)lives_malloc(format->buffer_size);
  ldev->buffer1.buffer_size = format->buffer_size;

  ldev->buffer2.data = (unsigned char *)lives_malloc(format->buffer_size);
  ldev->buffer2.buffer_size = format->buffer_size;

  ldev->buffer_ready = 0;
  ldev->fileno = mainw->current_file;

  cfile->bpp = format->bpp;

  unicap_start_capture(ldev->handle);

  // if it is greyscale, we will add fake U and V planes
  if (ldev->current_palette == WEED_PALETTE_A8) {
    ldev->current_palette = WEED_PALETTE_YUV444P;
    ldev->is_really_grey = TRUE;
  } else ldev->is_really_grey = FALSE;

  return TRUE;
}


void lives_vdev_free(lives_vdev_t *ldev) {
  if (!ldev) return;
  unicap_stop_capture(ldev->handle);
  unicap_unlock_stream(ldev->handle);
  unicap_close(ldev->handle);
  if (ldev->buffer1.data) lives_free(ldev->buffer1.data);
  if (ldev->buffer2.data) lives_free(ldev->buffer2.data);
}


boolean on_open_vdev_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  unicap_device_t devices[MAX_DEVICES];

  LiVESList *devlist = NULL;

  LiVESWidget *card_dialog;

  char *fname;

  int devno = 0;

  int new_file = mainw->first_free_file;
  int old_file = mainw->current_file;

  int response;

  int dev_count;
  int status = STATUS_SUCCESS;

  register int i;

  mainw->open_deint = FALSE;

  status = unicap_reenumerate_devices(&dev_count);

  if (dev_count == 0) {
    do_no_in_vdevs_error();
    return FALSE;
  }

  // get device list
  for (i = 0; SUCCESS(status) && (dev_count < MAX_DEVICES); i++) {
    status = unicap_enumerate_devices(NULL, &devices[i], i);
    if (!SUCCESS(status)) {
      if (i == 0) LIVES_INFO("Unicap failed to get any devices");
    }
  }

  if (!user_data) {
    for (i = 0; i < dev_count; i++) {
      if (!unicap_is_stream_locked(&devices[i])) {
        devlist = lives_list_prepend(devlist, devices[i].identifier);
      }
    }

    if (!devlist) {
      do_locked_in_vdevs_error();
      return FALSE;
    }

    mainw->fx1_val = 0;
    mainw->open_deint = FALSE;
    card_dialog = create_combo_dialog(1, (livespointer)devlist);
    response = lives_dialog_run(LIVES_DIALOG(card_dialog));
    lives_list_free(devlist); /// free only after runniong dialog !!!
    if (response == LIVES_RESPONSE_CANCEL) {
      return FALSE;
    }
    lives_widget_destroy(card_dialog);
  } else {
    char *device = (char *)user_data;
    for (i = 0; i < dev_count; i++) {
      if (!strcmp(device, devices[i].device)) {
        mainw->fx1_val = i;
        break;
      }
    }
  }

  for (i = dev_count - 1; i >= 0; i--) {
    if (!unicap_is_stream_locked(&devices[i])) {
      if (mainw->fx1_val == 0) {
        devno = i;
        break;
      }
    }
    mainw->fx1_val--;
  }

  if (*devices[devno].device) fname = lives_strdup(devices[devno].device);
  else fname = lives_strdup(devices[devno].identifier);

  if (!get_new_handle(new_file, fname)) {
    lives_free(fname);
    return FALSE;
  }

  mainw->current_file = new_file;
  cfile->clip_type = CLIP_TYPE_VIDEODEV;

  d_print(""); ///< force switchtext

  g_print("checking formats for %s\n", fname);

  if (!open_vdev_inner(&devices[devno])) {
    d_print(_("Unable to open device %s\n"), fname);
    lives_free(fname);
    close_current_file(old_file);
    return FALSE;
  }

  if (cfile->interlace != LIVES_INTERLACE_NONE && prefs->auto_deint) cfile->deinterlace = TRUE; ///< auto deinterlace
  if (!cfile->deinterlace) cfile->deinterlace = mainw->open_deint; ///< user can also force deinterlacing

  cfile->start = cfile->end = cfile->frames = 1;
  cfile->is_loaded = TRUE;
  add_to_clipmenu();

  lives_snprintf(cfile->type, 40, "%s", fname);

  d_print(_("Opened device %s\n"), devices[devno].identifier);

  switch_clip(0, new_file, TRUE);

  lives_free(fname);

  return TRUE;
}

#endif

