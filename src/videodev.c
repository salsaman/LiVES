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
#include "paramwindow.h"
#include "rfx-builder.h"

#include <unicap/unicap.h>

static lives_proc_thread_t ldev_free_lpt = NULL;

static lives_object_instance_t *lives_videodev_inst_create(uint64_t subtype);


static boolean lives_wait_user_buffer(lives_vdev_t *ldev, unicap_data_buffer_t **buff, double timeout) {
  // wait for USER type buffer
  unicap_status_t status;
  lives_alarm_t alarm_handle = lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);
  int ncount;

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
  } while (lives_alarm_check(alarm_handle) > 0);

  return FALSE;
}


static boolean lives_wait_system_buffer(lives_vdev_t *ldev, double timeout) {
  // wait for SYSTEM type buffer
  lives_alarm_t alarm_handle = lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);

  do {
    if (ldev->buffer_ready) {
      lives_alarm_clear(alarm_handle);
      return TRUE;
    }
    lives_nanosleep(1000);
  } while (lives_alarm_check(alarm_handle) > 0);
  lives_alarm_clear(alarm_handle);

  return FALSE;
}


static void new_frame_cb(unicap_event_t event, unicap_handle_t handle,
                         unicap_data_buffer_t *buffer, void *usr_data) {
  lives_vdev_t *ldev = (lives_vdev_t *)usr_data;
  if (!LIVES_IS_PLAYING || (mainw->playing_file != ldev->fileno
                            && mainw->blend_file != ldev->fileno)) {
    ldev->buffer_ready = 0;
    return;
  }
  lives_get_current_ticks();
  if (ldev->buffer_ready != 1) {
    lives_memcpy(ldev->buffer1.data, buffer->data, ldev->buffer1.buffer_size);
    ldev->buffer_ready = 1;
  } else {
    lives_memcpy(ldev->buffer2.data, buffer->data, ldev->buffer2.buffer_size);
    ldev->buffer_ready = 2;
  }
  mainw->force_show = TRUE;
}


boolean weed_layer_set_from_lvdev(weed_layer_t *layer, lives_clip_t *sfile, double timeoutsecs) {
  lives_vdev_t *ldev = (lives_vdev_t *)sfile->primary_src->source;
  unicap_data_buffer_t *returned_buffer = NULL;
  void **pixel_data;
  int nplanes;

  weed_layer_set_size(layer, sfile->hsize / weed_palette_get_pixels_per_macropixel(ldev->palette),
                      sfile->vsize);
  weed_layer_set_palette_yuv(layer, ldev->palette, ldev->YUV_clamping, ldev->YUV_subspace,
                             ldev->YUV_clamping);

  create_empty_pixel_data(layer, TRUE, TRUE);

  if (ldev->buffer_type == UNICAP_BUFFER_TYPE_USER) {
    if (!lives_wait_user_buffer(ldev, &returned_buffer, timeoutsecs)) {
#ifdef DEBUG_UNICAP
      lives_printerr("Failed to wait for user buffer!\n");
      unicap_stop_capture(ldev->handle);
      unicap_dequeue_buffer(ldev->handle, &returned_buffer);
      unicap_start_capture(ldev->handle);
#endif
      return FALSE;
    }
  } else {
    // wait for callback to fill buffer
    if (!ldev->buffer_ready && !lives_wait_system_buffer(ldev, timeoutsecs)) {
#ifdef DEBUG_UNICAP
      lives_printerr("Failed to wait for system buffer!\n");
#endif
    }
    if (ldev->buffer_ready == 1) returned_buffer = &ldev->buffer1;
    else returned_buffer = &ldev->buffer2;
  }

  pixel_data = weed_layer_get_pixel_data_planar(layer, &nplanes);

  if (nplanes > 1 && !ldev->is_really_grey) {
    boolean contig = FALSE;
    if (weed_get_boolean_value(layer, LIVES_LEAF_PIXEL_DATA_CONTIGUOUS, NULL) == WEED_TRUE) contig = TRUE;
    pixel_data_planar_from_membuf(pixel_data, returned_buffer->data, sfile->hsize * sfile->vsize, ldev->palette, contig);
  } else {
    if (ldev->buffer_type == UNICAP_BUFFER_TYPE_SYSTEM) {
      int rowstride = weed_layer_get_rowstride(layer);
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

  lives_free(pixel_data);

  if (ldev->buffer_type != UNICAP_BUFFER_TYPE_SYSTEM) {
    unicap_queue_buffer(ldev->handle, returned_buffer);
  }

  return TRUE;
}


static void canikill(lives_vdev_t *ldev) {
  // if the device is busy, we can try "fuser /dev/videoXXX"
  // return is e.g. '/dev/video0:         206494'
  // then parse the pid, and "kill -9 <PID>"
  char cbuf[1024];
  char *com = lives_strdup_printf("fuser %s 2>%s", ldev->fname, LIVES_DEVNULL);
  lives_popen(com, TRUE, cbuf, 1024);
  if (THREADVAR(com_failed)) {
    break_me("comf");
    THREADVAR(com_failed) = FALSE;
    lives_free(com);
  } else {
    int npids = get_token_count(cbuf, ' ') - 1;
    if (npids > 0) {
      char **pids = lives_strsplit(cbuf, " ", npids);
      for (int i = npids - 1; i >= 0; i--) {
        int pidd = atoi(pids[i]);
        if (pidd == capable->mainpid || !pidd) continue;
        if (do_yesno_dialogf("Can I free up %s by killing pid %d please ?",
                             ldev->fname, pidd)) {
          lives_kill((lives_pid_t)pidd, LIVES_SIGKILL);
          break;
        }
      }
      lives_strfreev(pids);
    }
  }
  lives_free(com);
}


static unicap_format_t *lvdev_get_best_format(const unicap_format_t *formats, lives_vdev_t *ldev,
    int palette, lives_match_t matmet, int width, int height) {
  // get nearest format for given palette, width and height
  // if palette is WEED_PALETTE_END, or cannot be matched, get best quality palette (preferring RGB)
  // width and height must be set, actual width and height will be set as near to this as possible
  // giving preference to larger frame size

  // if the device supports no usable formats, returns NULL

  // Note: we match first by palette, then by size

  unicap_format_t *format;
  int f = -1;
  int bestp = WEED_PALETTE_END;
  int bestw = 0, besth = 0;
  double cpbytes;
  int cpal;
  int format_count, i;

  // get details
  for (format_count = 0;
       SUCCESS(unicap_enumerate_formats(ldev->handle, NULL, (unicap_format_t *)&formats[format_count], format_count))
       && (format_count < MAX_FORMATS); format_count++) {
    format = (unicap_format_t *)&formats[format_count];

    // TODO - check if we need to free format->sizes

    // TODO - prefer non-interlaced, YCbCr for YUV
    cpal = fourccp_to_weedp(format->fourcc, format->bpp, NULL, NULL, NULL, NULL);

    g_print("Palette description is %s\n", format->identifier);

    if (cpal == WEED_PALETTE_END || weed_palette_is_alpha(cpal)) {
#ifdef DEBUG_UNICAP
      // set format to try and get more data
      unicap_set_format(ldev->handle, format);
      lives_printerr("Unusable palette with fourcc 0x%x  %d bpp=%d, size=%dx%d buf=%d\n", format->fourcc,
                     cpal, format->bpp, format->size.width, format->size.height, (int)format->buffer_size);
#endif
      continue;
    }

    cpbytes = weed_palette_get_bytes_per_pixel(cpal);

    if (bestp == WEED_PALETTE_END || cpal == palette || weed_palette_is_alpha(bestp) ||
        /// test if cpal is higher or same quality
        weed_palette_is_lower_quality(bestp, cpal) ||
        (weed_palette_is_yuv(bestp) && weed_palette_is_rgb(cpal))) {
      // got better palette, or exact match

      // prefer exact match on target palette if we have it
      if (palette != WEED_PALETTE_END && bestp == palette && cpal != palette) continue;

      // TODO - try to minimise aspect delta
      // for now we just go with the smallest size >= target (or largest frame size if none are >= target)

      if (matmet == LIVES_MATCH_HIGHEST
          || (width >= format->min_size.width && height >= format->min_size.height)) {
        if (format->h_stepping > 0 && format->v_stepping > 0) {
          if (matmet == LIVES_MATCH_HIGHEST) {
            width = format->max_size.width;
            height = format->max_size.height;
          }

#ifdef DEBUG_UNICAP
          lives_printerr("Can set any size with step %d and %d; min %d x %d, max %d x %d\n",
                         format->h_stepping, format->v_stepping,
                         format->min_size.width, format->min_size.height, format->max_size.width, format->max_size.height);
#endif
          if (matmet != LIVES_MATCH_HIGHEST) {
            // can set exact size (within stepping limits)
            format->size.width = (int)(((double)width + (double)format->h_stepping / 2.)
                                       / (double)format->h_stepping) * format->h_stepping;

            format->size.height = (int)(((double)height + (double)format->v_stepping / 2.)
                                        / (double)format->v_stepping) * format->v_stepping;

            if (format->size.width > format->max_size.width) format->size.width = format->max_size.width;
            if (format->size.height > format->max_size.height) format->size.height = format->max_size.height;
          }
          if (!SUCCESS(unicap_set_format(ldev->handle, format))) {
#ifdef DEBUG_UNICAP
            lives_printerr("Could not set Unicap format\n");
#endif
            canikill(ldev);
            continue;
          }
          if (format->buffer_size < (size_t)((double)(format->size.width * format->size.height) * cpbytes)) {
#ifdef DEBUG_UNICAP
            lives_printerr("Buffer size mismatch (%ld vs %ld, ratio %.2f) skipping 1\n",
                           format->buffer_size, (size_t)((double)(format->size.width * format->size.height) * cpbytes),
                           (double)format->buffer_size / ((double)(format->size.width * format->size.height) * cpbytes));
#endif
            continue;
          }

          if (format->size.width >= bestw || format->size.height >= besth) {
            bestp = cpal;
            bestw = format->size.width;
            besth = format->size.height;
            f = format_count;
          }
        } else {
          // array of sizes supported
          // step through sizes
#ifdef DEBUG_UNICAP
          lives_printerr("Checking %d array sizes with palette %d\n", format->size_count, cpal);
#endif

          if (format->size_count == 0) {
            // only one size we can use, this is it...
            if (!SUCCESS(unicap_set_format(ldev->handle, format))) {
#ifdef DEBUG_UNICAP
              lives_printerr("Could not set Unicap format\n");
#endif
              canikill(ldev);
              continue;
            }
            if (format->buffer_size < (size_t)((double)(format->size.width * format->size.height) * cpbytes)) {
#ifdef DEBUG_UNICAP
              lives_printerr("Buffer size mismatch (%ld vs %ld, ratio %.2f) skipping 3\n",
                             format->buffer_size, (size_t)((double)(format->size.width * format->size.height) * cpbytes),
                             (double)format->buffer_size / ((double)(format->size.width * format->size.height) * cpbytes));
#endif
              continue;
            }
            if ((format->size.width >= bestw || format->size.height >= besth)
                && (matmet == LIVES_MATCH_HIGHEST || (bestw <= width || besth <= height))) {
              // this format supports a better size match
              bestp = cpal;
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

            if (!SUCCESS(unicap_set_format(ldev->handle, format))) {
#ifdef DEBUG_UNICAP
              lives_printerr("Could not set Unicap format\n");
#endif
              canikill(ldev);
              continue;
            }
            if (format->buffer_size < (size_t)((double)(format->sizes[i].width * format->sizes[i].height) * cpbytes)) {
              size_t ss1 = format->buffer_size;
              size_t ss2 = (size_t)((double)(format->sizes[i].width * format->sizes[i].height) * cpbytes);


#ifdef DEBUG_UNICAP
              lives_printerr("Buffer size mismatch %ld and %ld (%ld vs %ld, ratio %.2f) skipping 2\n",
                             ss1, ss2, format->buffer_size, (size_t)((double)(format->sizes[i].width * format->sizes[i].height) * cpbytes),
                             (double)format->buffer_size / ((double)(format->sizes[i].width * format->sizes[i].height) * cpbytes));
#endif
              continue;
            }
            if (format->sizes[i].width >= bestw && format->sizes[i].height >= besth &&
                (matmet == LIVES_MATCH_HIGHEST || (bestw <= width || besth <= height))) {
              // this format supports a better size / palette match
              bestp = cpal;
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
        if (matmet == LIVES_MATCH_AT_MOST
            && (width < format->min_size.width || height < format->min_size.height)) continue; // TODO - minimise aspect delta
        bestp = cpal;
        bestw = format->size.width = format->min_size.width;
        besth = format->size.height = format->min_size.height;
        f = format_count;
      }
    }
  }

  if (f > -1) return (unicap_format_t *)(&formats[f]);
  return NULL;
}


void update_props_from_attributes(lives_vdev_t *ldev, lives_rfx_t *rfx) {
  unicap_property_t props[MAX_PROPS];
  unicap_lock_properties(ldev->handle);
  for (int prop_count = 0;
       SUCCESS(unicap_enumerate_properties(ldev->handle, NULL, (unicap_property_t *)&props[prop_count], prop_count))
       && (prop_count < MAX_PROPS); prop_count++) {
    unicap_property_t *prop = (unicap_property_t *)&props[prop_count];
    unicap_property_type_enum_t ptype = prop->type;
    if (ptype == UNICAP_PROPERTY_TYPE_DATA || ptype == UNICAP_PROPERTY_TYPE_FLAGS || ptype == UNICAP_PROPERTY_TYPE_UNKNOWN
        || ptype == UNICAP_PROPERTY_TYPE_MENU || ptype == UNICAP_PROPERTY_TYPE_VALUE_LIST) continue;
    //if (ptype != UNICAP_PROPERTY_TYPE_RANGE) continue;
    lives_obj_attr_t *attr = lives_object_get_attribute(ldev->object, prop->identifier);
    if (attr) {
      lives_param_t *param;
      double val;
      //if (lives_attribute_is_readonly(ldev->object, prop->identifier)) continue;
      param = find_rfx_param_by_name(rfx, prop->identifier);
      if (param->type == LIVES_PARAM_TYPE_UNDEFINED) continue;
      if (!(param->flags & PARAM_FLAG_VALUE_SET)) continue;
      if (param->type == LIVES_PARAM_BOOL)
        unicap_set_property_value(ldev->handle, prop->identifier, (double)weed_get_boolean_value(attr, WEED_LEAF_VALUE, NULL));
      else if (param->type == LIVES_PARAM_NUM && param->dp == 0)
        unicap_set_property_value(ldev->handle, prop->identifier, get_int_param(param->value));
      else unicap_set_property_value(ldev->handle, prop->identifier, get_double_param(param->value));
      unicap_get_property_value(ldev->handle, prop->identifier, &val);

      if (param->type == LIVES_PARAM_BOOL || (param->type == LIVES_PARAM_NUM && param->dp == 0)) {
        set_int_param(param->value, (int)val);
        lives_object_set_attribute_value(ldev->object, prop->identifier, (int)val);
      } else {
        set_double_param(param->value, val);
        lives_object_set_attribute_value(ldev->object, prop->identifier, val);
      }
    }
  }
  unicap_unlock_properties(ldev->handle);
}


static void set_palette_desc(lives_object_t *obj, lives_obj_attr_t *attr) {
  lives_param_t *rpar;
  lives_vdev_t *ldev = (lives_vdev_t *)obj->priv;
  char *palname = weed_palette_get_name_full(ldev->palette, ldev->YUV_clamping, ldev->YUV_subspace);
  weed_plant_t *gui = weed_get_plantptr_value(attr, WEED_LEAF_GUI, NULL);
  if (!gui) {
    gui = weed_plant_new(WEED_PLANT_GUI);
    weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
  }
  weed_set_string_value(gui, LIVES_LEAF_DISPVAL, palname);
  rpar = weed_get_voidptr_value(gui, LIVES_LEAF_RPAR, NULL);
  if (rpar && rpar->widgets && rpar->widgets[0] && LIVES_IS_LABEL(rpar->widgets[0]))
    lives_label_set_text(LIVES_LABEL(rpar->widgets[0]), palname);
  if (palname) lives_free(palname);
}


static void *lives_ldev_free_cb(lives_object_t *obj, void *data) {
  if (data) {
    lives_vdev_t **pldev = (lives_vdev_t **)data;
    lives_vdev_free(*pldev);
    *pldev = NULL;
  }
  return NULL;
}


/// get devnumber from user and open it to a new clip

static boolean open_vdev_inner(unicap_device_t *device, lives_match_t matmet, boolean adj) {
  // create a virtual clip
  lives_obj_attr_t *attr;
  lives_vdev_t *ldev = (lives_vdev_t *)lives_malloc(sizeof(lives_vdev_t));
  unicap_format_t formats[MAX_FORMATS];
  unicap_property_t props[MAX_PROPS];
  lives_object_t *obj;
  lives_rfx_t *rfx;
  double cpbytes;
  int prop_count, nprops;

  // make sure we close the stream even on abort
  ldev_free_lpt = lives_hook_append(mainw->global_hook_stacks, FATAL_HOOK, 0, lives_ldev_free_cb, (void *)&ldev);

  // open dev
  unicap_open(&ldev->handle, device);

  //check return value and take appropriate action
  if (!ldev->handle) {
    LIVES_ERROR("vdev input: cannot open device");
    lives_free(ldev);
    return FALSE;
  }

  if (*device->device) ldev->fname = lives_strdup(device->device);
  else ldev->fname = lives_strdup(device->identifier);

  unicap_lock_stream(ldev->handle);

  ldev->format = lvdev_get_best_format(formats, ldev, WEED_PALETTE_END,
                                       matmet, DEF_GEN_WIDTH, DEF_GEN_HEIGHT);

  if (!ldev->format) {
    lives_hook_remove(ldev_free_lpt);
    LIVES_INFO("No useful formats found");
    unicap_unlock_stream(ldev->handle);
    unicap_close(ldev->handle);
    lives_free(ldev);
    return FALSE;
  }

  if (!(ldev->format->buffer_types & UNICAP_BUFFER_TYPE_USER)) {
    // use system buffer type
    g_print("NB is %d\n", ldev->format->system_buffer_count);
    ldev->format->buffer_type = UNICAP_BUFFER_TYPE_SYSTEM;

    cfile->delivery = LIVES_DELIVERY_PUSH_PULL;

    // set a callback for new frame
    unicap_register_callback(ldev->handle, UNICAP_EVENT_NEW_FRAME, (unicap_callback_t) new_frame_cb,
                             (void *) ldev);
    /* unicap_register_callback(ldev->handle, UNICAP_EVENT_DROP_FRAME, (unicap_callback_t) drop_frame_cb, */
    /*                          (void *) ldev); */

  } else ldev->format->buffer_type = UNICAP_BUFFER_TYPE_USER;

  ldev->buffer_type = ldev->format->buffer_type;

  // ignore YUV subspace for now
  ldev->palette = fourccp_to_weedp(ldev->format->fourcc, ldev->format->bpp, (int *)&cfile->interlace,
                                   &ldev->YUV_sampling, &ldev->YUV_subspace, &ldev->YUV_clamping);
  cpbytes = weed_palette_get_bytes_per_pixel(ldev->palette);

#ifdef DEBUG_UNICAP
  lives_printerr("\nUsing palette with fourcc 0x%x, translated as %s\n", ldev->format->fourcc,
                 weed_palette_get_name(ldev->palette));
#endif

  if (!SUCCESS(unicap_set_format(ldev->handle, ldev->format))) {
    lives_hook_remove(ldev_free_lpt);
    LIVES_ERROR("Unicap error setting format");
    unicap_unlock_stream(ldev->handle);
    unicap_close(ldev->handle);
    lives_free(ldev);
    return FALSE;
  }

  g_print("ALLX %ld %d %d %d %d\n", ldev->format->buffer_size, ldev->format->size.width, ldev->format->size.height,
          weed_palette_get_bits_per_macropixel(
            ldev->palette), weed_palette_get_pixels_per_macropixel(ldev->palette));

  if (ldev->format->buffer_size < (size_t)((double)(ldev->format->size.width * ldev->format->size.height) * cpbytes)) {
    int wwidth = ldev->format->size.width, awidth;
    int wheight = ldev->format->size.height, aheight;
    // something went wrong setting the size - the buffer is wrongly sized
#ifdef DEBUG_UNICAP
    lives_printerr("Unicap buffer size is wrong, resetting it.\n");
#endif
    // get the size again

    unicap_get_format(ldev->handle, ldev->format);
    awidth = ldev->format->size.width;
    aheight = ldev->format->size.height;

#ifdef DEBUG_UNICAP
    lives_printerr("Wanted frame size %d x %d, got %d x %d\n", wwidth, wheight, awidth, aheight);
#endif

    ldev->format->buffer_size = (size_t)((double)(ldev->format->size.width * ldev->format->size.height) * cpbytes);
  }

  cfile->hsize = ldev->format->size.width;
  cfile->vsize = ldev->format->size.height;

  obj = ldev->object = lives_videodev_inst_create(VIDEO_DEV_UNICAP);
  obj->priv = (void *)ldev;

  lives_object_set_attribute_value(obj, VDEV_PROP_WIDTH, cfile->hsize);
  //lives_attribute_set_readonly(obj, VDEV_PROP_WIDTH, TRUE);

  lives_object_set_attribute_value(obj, VDEV_PROP_HEIGHT, cfile->vsize);
  //lives_attribute_set_readonly(obj, VDEV_PROP_HEIGHT, TRUE);

  cfile->primary_src = add_clip_source(mainw->current_file, -1, SRC_PURPOSE_PRIMARY, (void *)ldev,
                                       LIVES_SRC_TYPE_DEVICE);

  ldev->buffer1.data = (unsigned char *)lives_malloc(ldev->format->buffer_size);
  ldev->buffer1.buffer_size = ldev->format->buffer_size;
  ldev->buffer2.data = (unsigned char *)lives_malloc(ldev->format->buffer_size);
  ldev->buffer2.buffer_size = ldev->format->buffer_size;

  if (ldev->buffer_type == UNICAP_BUFFER_TYPE_USER) {
    unicap_queue_buffer(ldev->handle, &ldev->buffer1);
    unicap_queue_buffer(ldev->handle, &ldev->buffer2);
  }

  ldev->buffer_ready = 0;
  ldev->fileno = mainw->current_file;

  cfile->bpp = ldev->format->bpp;

  unicap_reenumerate_properties(ldev->handle, &nprops);

  //lives_attribute_set_readonly(obj, VDEV_PROP_WIDTH, TRUE);
  lives_attribute_set_param_type(obj, VDEV_PROP_WIDTH, _("Width"), WEED_PARAM_INTEGER);

  //lives_attribute_set_readonly(obj, VDEV_PROP_HEIGHT, TRUE);
  lives_attribute_set_param_type(obj, VDEV_PROP_HEIGHT, _("Height"), WEED_PARAM_INTEGER);

  for (prop_count = 0;
       SUCCESS(unicap_enumerate_properties(ldev->handle, NULL, (unicap_property_t *)&props[prop_count], prop_count))
       && (prop_count < MAX_PROPS); prop_count++) {
    unicap_property_t *prop = (unicap_property_t *)&props[prop_count];
    unicap_property_flags_t flags = prop->flags;
    unicap_property_flags_t mask = prop->flags_mask;

    // if flags & UNICAP_FLAGS_READ_OUT --> create out param
    if (!lives_strcmp(prop->identifier, "frame rate")) {
      cfile->pb_fps = cfile->fps = prop->value;
      cfile->target_framerate = cfile->fps;
      lives_object_set_attribute_value(obj, VDEV_PROP_FPS, cfile->fps);
      //lives_attribute_set_readonly(obj, VDEV_PROP_FPS, TRUE);
      lives_attribute_set_param_type(obj, VDEV_PROP_FPS, _("FPS"), WEED_PARAM_FLOAT);
    } else {
      boolean valid = TRUE;
      double val;
      if ((flags & UNICAP_FLAGS_ON_OFF) && (mask & UNICAP_FLAGS_ON_OFF)) {
        attr = lives_object_declare_attribute(obj, prop->identifier, WEED_SEED_BOOLEAN);
        unicap_get_property_value(ldev->handle, prop->identifier, &val);
        lives_object_set_attribute_value(obj, prop->identifier,
                                         val == 0. ? WEED_FALSE : WEED_TRUE);
        lives_attribute_set_param_type(obj, prop->identifier, prop->identifier, WEED_PARAM_SWITCH);
      } else {
        unicap_property_type_enum_t ptype = prop->type;
        switch (ptype) {
        case UNICAP_PROPERTY_TYPE_DATA:
        case UNICAP_PROPERTY_TYPE_FLAGS:
        case UNICAP_PROPERTY_TYPE_UNKNOWN:
          valid = FALSE;
          break;
        case UNICAP_PROPERTY_TYPE_VALUE_LIST:
        case UNICAP_PROPERTY_TYPE_MENU: {
          unicap_property_value_list_t vlist;
          unicap_property_menu_t menu;
          char **choices;
          int n_choices;
          weed_plant_t *gui;
          if (ptype == UNICAP_PROPERTY_TYPE_MENU) {
            menu = prop->menu;
            choices = menu.menu_items;
            n_choices = menu.menu_item_count;
          } else {
            vlist = prop->value_list;
            n_choices = vlist.value_count;
            choices = lives_calloc(n_choices, sizeof(char *));
            for (int i = 0; i < n_choices; i++) {
              choices[i] = lives_strdup_printf("%f", vlist.values[i]);
            }
          }
          // choices with menu_items and menu_item_count
          attr = lives_object_declare_attribute(obj, prop->identifier, WEED_SEED_INT);
          lives_object_set_attribute_value(obj, prop->identifier, 0);
          gui = weed_plant_new(WEED_PLANT_GUI);
          weed_set_plantptr_value(attr, WEED_LEAF_GUI, gui);
          weed_set_string_array(gui, WEED_LEAF_CHOICES, n_choices, choices);
          lives_attribute_set_param_type(obj, prop->identifier, prop->identifier, WEED_PARAM_INTEGER);
          break;
        }
        default:
          attr = lives_object_declare_attribute(obj, prop->identifier, WEED_SEED_DOUBLE);
          unicap_get_property_value(ldev->handle, prop->identifier, &val);
          lives_object_set_attribute_value(obj, prop->identifier, val);
          if (ptype == UNICAP_PROPERTY_TYPE_RANGE) {
            unicap_property_range_t range = prop->range;
            weed_set_double_value(attr, WEED_LEAF_MIN, range.min);
            weed_set_double_value(attr, WEED_LEAF_MAX, range.max);
          }
          lives_attribute_set_param_type(obj, prop->identifier, prop->identifier, WEED_PARAM_FLOAT);
          break;
        }
      }
      if (!valid) continue;
    }
    /* if ((flags & UNICAP_FLAGS_AUTO) && (mask & UNICAP_FLAGS_AUTO)) */
    /*   lives_attribute_set_readonly(obj, prop->identifier, TRUE); */
    /* else if ((flags & UNICAP_FLAGS_READ_ONLY) && (mask & UNICAP_FLAGS_READ_ONLY)) */
    /*   lives_attribute_set_readonly(obj, prop->identifier, TRUE); */
  }

  // if it is greyscale, we will add fake U and V planes
  if (ldev->palette == WEED_PALETTE_A8) {
    ldev->palette = WEED_PALETTE_YUV444P;
    ldev->is_really_grey = TRUE;
  } else ldev->is_really_grey = FALSE;

  //lives_attribute_append_listener(obj, VDEV_PROP_PALETTE, set_palette_desc);
  lives_attribute_set_param_type(obj, VDEV_PROP_PALETTE, _("Colourspace"), WEED_PARAM_INTEGER);
  lives_object_set_attribute_value(obj, VDEV_PROP_PALETTE, ldev->palette);
  // lives_attribute_set_readonly(obj, VDEV_PROP_PALETTE, TRUE);

  //
  rfx = obj_attrs_to_rfx(obj, FALSE);

  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p0|p1|"));
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p2|"));
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|p3|"));
  rfx->gui_strings = lives_list_append(rfx->gui_strings, lives_strdup("layout|hseparator|"));

  if (adj) {
    LiVESWidget *dialog = rfx_make_param_dialog(rfx, _("Webcam Settings"), TRUE);
    if (lives_dialog_run(LIVES_DIALOG(dialog)) == LIVES_RESPONSE_OK) {
      update_props_from_attributes(ldev, rfx);
      lives_widget_destroy(dialog);
    }
  }

  unicap_start_capture(ldev->handle);

  return TRUE;
}


void lives_vdev_free(lives_vdev_t *ldev) {
  if (!ldev) return;
  lives_hook_remove(ldev_free_lpt);
  if (ldev->format->buffer_type == UNICAP_BUFFER_TYPE_SYSTEM)
    unicap_unregister_callback(ldev->handle, UNICAP_EVENT_NEW_FRAME);
  unicap_stop_capture(ldev->handle);
  unicap_unlock_stream(ldev->handle);
  unicap_close(ldev->handle);
  if (ldev->buffer1.data) {
    lives_free(ldev->buffer1.data);
    ldev->buffer1.data = NULL;
  }
  if (ldev->buffer2.data) {
    lives_free(ldev->buffer2.data);
    ldev->buffer2.data = NULL;
  }
  ldev->object->priv = NULL;
  lives_object_instance_destroy(ldev->object);
  lives_free(ldev);
}


boolean on_open_vdev_activate(LiVESMenuItem *menuitem, const char *devname) {
  unicap_device_t devices[MAX_DEVICES];

  LiVESList *devlist = NULL;
  LiVESSList *rbgroup;

  LiVESWidget *card_dialog;
  LiVESWidget *dialog_vbox;
  LiVESWidget *layout, *hbox;

  char *fname;
  char mopts[N_MATCH_TYPES];

  LiVESResponseType response = LIVES_RESPONSE_NONE;
  lives_match_t matmet;

  int imatmet;
  int new_file = mainw->first_free_file;
  int old_file = mainw->current_file;
  int dev_count, devno = 0;
  int status = STATUS_SUCCESS;

  int i;

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

  if (!devname) {
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
    dialog_vbox = lives_dialog_get_content_area(LIVES_DIALOG(card_dialog));

    lives_dialog_add_button_from_stock(LIVES_DIALOG(card_dialog),
                                       "settings", _("Adjust Settings"), LIVES_RESPONSE_SHOW_DETAILS);

    layout = lives_layout_new(NULL);
    lives_standard_expander_new(_("Options"), _("Hide options"), LIVES_BOX(dialog_vbox), layout);

    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);
    lives_layout_add_row(LIVES_LAYOUT(layout));
    lives_layout_add_label(LIVES_LAYOUT(layout), _("Desired frame size:"), FALSE);

    lives_memset(mopts, 0, N_MATCH_TYPES);
    mopts[LIVES_MATCH_AT_MOST] = mopts[LIVES_MATCH_HIGHEST] = MATCH_TYPE_ENABLED;
    if (mopts[prefs->webcam_matmet]) mopts[prefs->webcam_matmet] = MATCH_TYPE_DEFAULT;
    else mopts[LIVES_MATCH_AT_MOST] = MATCH_TYPE_DEFAULT;

    rbgroup = add_match_methods(LIVES_LAYOUT(layout), mopts, 4, 4, FALSE);

    lives_layout_add_separator(LIVES_LAYOUT(layout), FALSE);
    lives_layout_add_fill(LIVES_LAYOUT(layout), FALSE);

    hbox = lives_layout_row_new(LIVES_LAYOUT(layout));
    add_deinterlace_checkbox(LIVES_BOX(hbox));

    add_fill_to_box(LIVES_BOX(dialog_vbox));
    add_fill_to_box(LIVES_BOX(dialog_vbox));

    response = lives_dialog_run(LIVES_DIALOG(card_dialog));
    lives_list_free(devlist); /// free only after runniong dialog !!!
    if (response == LIVES_RESPONSE_CANCEL) return FALSE;
    imatmet = rbgroup_get_data(rbgroup, MATCHTYPE_KEY, LIVES_MATCH_UNDEFINED);
    lives_widget_destroy(card_dialog);
    if (imatmet < 0) {
      matmet = (lives_match_t)(-imatmet);
      update_int_pref(PREF_WEBCAM_MATMET, matmet, TRUE);
    } else matmet = (lives_match_t)imatmet;
  } else {
    matmet = prefs->webcam_matmet;
    for (i = 0; i < dev_count; i++) {
      if (!lives_strcmp(devname, devices[i].device)) {
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

  if (!open_vdev_inner(&devices[devno], matmet, response == LIVES_RESPONSE_SHOW_DETAILS)) {
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


/// objects / intents etc
static lives_object_instance_t *lives_videodev_inst_create(uint64_t subtype) {
  lives_object_instance_t *inst = lives_object_instance_create(OBJECT_TYPE_MEDIA_SOURCE, subtype);
  inst->state = OBJECT_STATE_NORMAL;
  lives_object_declare_attribute(inst, VDEV_PROP_WIDTH, WEED_SEED_INT);
  lives_object_declare_attribute(inst, VDEV_PROP_HEIGHT, WEED_SEED_INT);
  lives_object_declare_attribute(inst, VDEV_PROP_PALETTE, WEED_SEED_INT);
  lives_object_declare_attribute(inst, VDEV_PROP_FPS, WEED_SEED_DOUBLE);

  // other attributes defined by the device itself

  return inst;
}

#endif

