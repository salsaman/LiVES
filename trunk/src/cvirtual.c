// cvirtual.c
// LiVES
// (c) G. Finch 2008 - 2016 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling "virtual" clips (CLIP_TYPE_FILE)

#include "main.h"

// frame_index is for files of type CLIP_TYPE_FILE
// a positive number is a pointer to a frame within the video file
// -1 means frame is stored as the corresponding image file
// e.g 00000001.jpg or 00000010.png etc.

#include "resample.h"


/** count virtual frames between start and end (inclusive) */
int count_virtual_frames(int *findex, int start, int end) {
  register int i;
  int count = 0;
  for (i = start - 1; i < end; i++) if (findex[i] != -1) count++;
  return count;
}


void create_frame_index(int fileno, boolean init, int start_offset, int nframes) {
  register int i;
  lives_clip_t *sfile = mainw->files[fileno];
  if (sfile == NULL || sfile->frame_index != NULL) return;

  sfile->frame_index = (int *)lives_malloc(nframes * sizint);

  if (init) {
    for (i = 0; i < sfile->frames; i++) {
      sfile->frame_index[i] = i + start_offset;
    }
  }
}


// save frame_index to disk
boolean save_frame_index(int fileno) {
  int fd, i;
  int retval;
  char *fname;
  lives_clip_t *sfile = mainw->files[fileno];

  if (fileno == 0) return TRUE;

  if (sfile == NULL || sfile->frame_index == NULL) return FALSE;

  fname = lives_build_filename(prefs->workdir, sfile->handle, "file_index", NULL);

  do {
    retval = 0;
    fd = lives_creat_buffered(fname, DEF_FILE_PERMS);
    if (fd < 0) {
      retval = do_write_failed_error_s_with_retry(fname, lives_strerror(errno), NULL);
    } else {
      mainw->write_failed = FALSE;
      for (i = 0; i < sfile->frames; i++) {
        lives_write_le_buffered(fd, &sfile->frame_index[i], 4, TRUE);
        if (mainw->write_failed) break;
      }

      lives_close_buffered(fd);

      if (mainw->write_failed) {
        retval = do_write_failed_error_s_with_retry(fname, NULL, NULL);
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(fname);

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

  return TRUE;
}


// load frame_index from disk
// returns -1 (error)
// or maxframe pointed to in clip

int load_frame_index(int fileno) {
  lives_clip_t *sfile = mainw->files[fileno];

  char *fname;

  int fd;
  int retval;
  int maxframe = 0;

  register int i;

  if (sfile == NULL || sfile->frame_index != NULL) return -1;

  lives_freep((void **)&sfile->frame_index);

  fname = lives_build_filename(prefs->workdir, sfile->handle, "file_index", NULL);

  if (!lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
    lives_free(fname);
    return 0;
  }

  do {
    retval = 0;

    fd = lives_open_buffered_rdonly(fname);

    if (fd < 0) {
      retval = do_read_failed_error_s_with_retry(fname, lives_strerror(errno), NULL);
      if (retval == LIVES_RESPONSE_CANCEL) {
        lives_free(fname);
        return -1;
      }
    } else {
      create_frame_index(fileno, FALSE, 0, sfile->frames);

      mainw->read_failed = FALSE;
      for (i = 0; i < sfile->frames; i++) {
        lives_read_le_buffered(fd, &sfile->frame_index[i], 4, FALSE);
        if (mainw->read_failed) break;
        if (sfile->frame_index[i] > maxframe) maxframe = sfile->frame_index[i];
      }

      lives_close_buffered(fd);

      if (mainw->read_failed) {
        mainw->read_failed = FALSE;
        retval = do_read_failed_error_s_with_retry(fname, NULL, NULL);
      }

    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(fname);

  return ++maxframe;
}


void del_frame_index(lives_clip_t *sfile) {
  // physically delete the frame_index for a clip
  // only done once all

  char *idxfile;

  register int i;

  // cannot call check_if_non_virtual() else we end up recursing

  if (sfile->frame_index != NULL) {
    for (i = 1; i <= sfile->frames; i++) {
      if (sfile->frame_index[i - 1] != -1) {
        LIVES_ERROR("deleting frame_index with virtual frames in it !");
        return;
      }
    }
  }

  if (sfile != clipboard) {
    idxfile = lives_build_filename(prefs->workdir, sfile->handle, "file_index", NULL);
    lives_rm(idxfile);
    lives_free(idxfile);
  }

  lives_freep((void **)&sfile->frame_index);
}


boolean check_clip_integrity(int fileno, const lives_clip_data_t *cdata) {
  lives_clip_t *sfile = mainw->files[fileno];

  lives_image_type_t empirical_img_type = sfile->img_type;

  int first_real_frame = 0;

  register int i;

  // check clip integrity upon loading

  // check that cached values match with sfile (on disk) values
  // TODO: also check sfile->frame_index to make sure all frames are present

  // return FALSE if we find any omissions/inconsistencies

  // check the image type
  for (i = 0; i < sfile->frames; i++) {
    if (sfile->frame_index[i] == -1) {
      // this is a non-virtual frame
      char *frame = make_image_file_name(sfile, i + 1, LIVES_FILE_EXT_PNG);
      if (lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) empirical_img_type = IMG_TYPE_PNG;
      else empirical_img_type = IMG_TYPE_JPEG;
      lives_free(frame);
      first_real_frame = i + 1;
      break;
    }
  }

  // TODO *** check frame count

  if (sfile->frames > 0 && (sfile->hsize * sfile->vsize == 0)) {
    if (first_real_frame > 0) {
      sfile->img_type = empirical_img_type;
      get_frames_sizes(fileno, first_real_frame);
    } else {
      if (!prefs->auto_nobord) {
        sfile->hsize = cdata->frame_width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
        sfile->vsize = cdata->frame_height;
      } else {
        sfile->hsize = cdata->width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
        sfile->vsize = cdata->height;
      }
    }
    goto mismatch;
  }

  if (sfile->fps != (double)cdata->fps) goto mismatch;

  if (sfile->img_type != empirical_img_type) sfile->img_type = empirical_img_type;

  // and all else are equal
  return TRUE;

mismatch:
  // something mismatched - trust the disk version
  ((lives_clip_data_t *)cdata)->fps = sfile->pb_fps = sfile->fps;

  sfile->img_type = empirical_img_type;

  sfile->needs_update = TRUE;

  return FALSE;
}


boolean check_if_non_virtual(int fileno, int start, int end) {
  // check if there are no virtual frames from start to end inclusive in clip fileno

  register int i;
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;

  if (sfile->clip_type != CLIP_TYPE_FILE) return TRUE;

  if (sfile->frame_index != NULL) {
    for (i = start; i <= end; i++) {
      if (sfile->frame_index[i - 1] != -1) return FALSE;
    }
  }

  if (start > 1 || end < sfile->frames) return TRUE;

  // no virtual frames in entire clip - change to CLIP_TYPE_DISK

  sfile->clip_type = CLIP_TYPE_DISK;
  del_frame_index(sfile);

  if (sfile->interlace != LIVES_INTERLACE_NONE) {
    sfile->interlace = LIVES_INTERLACE_NONE; // all frames should have been deinterlaced
    sfile->deinterlace = FALSE;
    if (fileno > 0) {
      save_clip_value(fileno, CLIP_DETAILS_INTERLACE, &sfile->interlace);
      if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
      if (bad_header) do_header_write_error(fileno);
    }
  }

  return TRUE;
}


boolean virtual_to_images(int sfileno, int sframe, int eframe, boolean update_progress, LiVESPixbuf **pbr) {
  // pull frames from a clip to images
  // from sframe to eframe inclusive (first frame is 1)

  // if update_progress, set mainw->msg with number of frames pulled

  // should be threadsafe apart from progress update

  // if pbr is non-null, it will be set to point to the pulled pixbuf (

  // return FALSE on write error

  register int i;
  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESPixbuf *pixbuf = NULL;
  LiVESError *error = NULL;
  char *oname;
  int retval;

  int progress = 1;

  if (sframe < 1) sframe = 1;

  for (i = sframe; i <= eframe; i++) {
    if (i > sfile->frames) break;

    if (update_progress) {
      threaded_dialog_spin(0.);
      lives_widget_context_update();
    }

    if (sfile->frame_index[i - 1] >= 0) {
      oname = NULL;

      if (pbr != NULL && pixbuf != NULL) lives_object_unref(pixbuf);

      pixbuf = pull_lives_pixbuf_at_size(sfileno, i, get_image_ext_for_type(sfile->img_type),
                                         q_gint64((i - 1.) / sfile->fps, sfile->fps), sfile->hsize, sfile->vsize, LIVES_INTERP_BEST);

      oname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));

      do {
        retval = 0;
        lives_pixbuf_save(pixbuf, oname, sfile->img_type, 100 - prefs->ocp, TRUE, &error);
        if (error != NULL && pbr == NULL) {
          retval = do_write_failed_error_s_with_retry(oname, error->message, NULL);
          lives_error_free(error);
          error = NULL;
        }
      } while (retval == LIVES_RESPONSE_RETRY);

      lives_freep((void **)&oname);

      if (pbr == NULL) {
        if (pixbuf != NULL) lives_object_unref(pixbuf);
        pixbuf = NULL;
      }

      if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

      // another thread may have called check_if_non_virtual - TODO : use a mutex
      if (sfile->frame_index == NULL) break;
      sfile->frame_index[i - 1] = -1;

      if (update_progress) {
        // sig_progress...
        lives_snprintf(mainw->msg, 256, "%d", progress++);
        threaded_dialog_spin(0.);
        lives_widget_context_update();
      }

      if (mainw->cancelled != CANCEL_NONE) {
        if (!check_if_non_virtual(sfileno, 1, sfile->frames)) save_frame_index(sfileno);
        if (pbr != NULL) *pbr = pixbuf;
        return TRUE;
      }
    }
  }

  if (pbr != NULL) *pbr = pixbuf;

  if (!check_if_non_virtual(sfileno, 1, sfile->frames)) if (!save_frame_index(sfileno)) return FALSE;

  return TRUE;
}


void insert_images_in_virtual(int sfileno, int where, int frames, int *frame_index, int start) {
  // insert physical (frames) images (or virtual possibly) into sfile at position where [0 = before first frame]
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];
  int nframes = sfile->frames;

  register int i, j = start - 1;

  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  create_frame_index(sfileno, FALSE, 0, nframes + frames);

  for (i = 0; i < where; i++) {
    sfile->frame_index[i] = sfile->frame_index_back[i];
  }

  for (i = where; i < where + frames; i++) {
    if (frame_index != NULL && frame_index[j] != -1) sfile->frame_index[i] = frame_index[j];
    else sfile->frame_index[i] = -1;
    if (++j >= clipboard->frames) j = 0;
  }

  for (i = where + frames; i < nframes + frames; i++) {
    sfile->frame_index[i] = sfile->frame_index_back[i - frames];
  }

  sfile->frames += frames;
  save_frame_index(sfileno);
  sfile->frames -= frames;
}


void delete_frames_from_virtual(int sfileno, int start, int end) {
  // delete (frames) images from sfile at position start to end
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  register int i;
  lives_clip_t *sfile = mainw->files[sfileno];
  int nframes = sfile->frames, frames = end - start + 1;

  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  if (nframes - frames == 0) {
    del_frame_index(sfile);
    return;
  }

  create_frame_index(sfileno, FALSE, 0, nframes - frames);

  for (i = 0; i < start - 1; i++) {
    sfile->frame_index[i] = sfile->frame_index_back[i];
  }

  for (i = end; i < nframes; i++) {
    sfile->frame_index[i - frames] = sfile->frame_index_back[i];
  }
  save_frame_index(sfileno);
}


void reverse_frame_index(int sfileno) {
  // reverse order of (virtual) frames in clip (only used fro clipboard)
  lives_clip_t *sfile = mainw->files[sfileno];
  int bck;
  register int i;

  if (sfile == NULL || sfile->frame_index == NULL) return;

  for (i = 0; i < sfile->frames >> 1; i++) {
    bck = sfile->frame_index[i];
    sfile->frame_index[i] = sfile->frame_index[sfile->frames - 1 - i];
    sfile->frame_index[sfile->frames - 1 - i] = bck;
  }
}


void restore_frame_index_back(int sfileno) {
  // undo an operation
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];

  lives_freep((void **)&sfile->frame_index);

  sfile->frame_index = sfile->frame_index_back;
  sfile->frame_index_back = NULL;

  if (sfile->frame_index != NULL) {
    sfile->clip_type = CLIP_TYPE_FILE;
    save_frame_index(sfileno);
  } else {
    del_frame_index(sfile);
    sfile->clip_type = CLIP_TYPE_DISK;
  }
}


void clean_images_from_virtual(lives_clip_t *sfile, int oldsframe, int oldframes) {
  // remove images on disk where the frame_index points to a frame in
  // the original clip

  // only needed if frames were reordered when rendered and the process is
  // then undone

  // oldsframe is > 1 if we rendered to a selection

  // should be threadsafe, provided the frame_index does not change

  // the only purpose of this is to reclaim disk space

  register int i;
  char *iname = NULL;

  if (sfile == NULL || sfile->frame_index == NULL) return;

  for (i = oldsframe; i <= oldframes; i++) {
    threaded_dialog_spin(0.);
    lives_widget_context_update();
    threaded_dialog_spin(0.);

    if ((i <= sfile->frames && sfile->frame_index[i - 1] != -1) || i > sfile->frames) {
      iname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      lives_rm(iname);
    }
  }
}


int *frame_index_copy(int *findex, int nframes, int offset) {
  // like it says on the label
  // copy first nframes from findex and return them
  // no checking is done to make sure nframes is in range

  // start at frame offset

  int *findexc = (int *)lives_malloc(sizint * nframes);
  register int i;

  for (i = 0; i < nframes; i++) findexc[i] = findex[i + offset];

  return findexc;
}


boolean is_virtual_frame(int sfileno, int frame) {
  // frame is virtual if it is still inside a video clip (read only)
  // once a frame is on disk as an image it is no longer virtual

  // frame starts at 1 here

  // a CLIP_TYPE_FILE with no virtual frames becomes a CLIP_TYPE_DISK

  lives_clip_t *sfile = mainw->files[sfileno];
  if (sfile->frame_index == NULL) return FALSE;
  if (sfile->frame_index[frame - 1] != -1) return TRUE;
  return FALSE;
}

/// experimental feature
#define TEST_TRANSCODE
#ifdef TEST_TRANSCODE

#if HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>
#else
#include "../libweed/weed.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#include "effects-weed.h"

boolean transcode(int start, int end) {
  int fd = -1;
  int resp;
  int asigned, aendian;
  int weed_error;
  int nsamps;
  
  register int i;
  
  boolean audio = FALSE;
  boolean swap_endian;
  boolean error = FALSE;
  
  int64_t currticks;

  ssize_t in_bytes;
  
  const char *img_ext;

  char *afname = NULL;
  
  double spf;
  
  _vid_playback_plugin *vpp = open_vid_playback_plugin("libav_stream", TRUE), *ovpp;
  _vppaw *vppa;
  weed_plant_t *frame_layer = NULL;

  void *abuff = NULL;

  void **pd_array;
  
  short *sbuff = NULL;
  
  float **fltbuf = NULL;
  
  if (vpp == NULL) return FALSE;
  
  memset(future_prefs->vpp_name, 0, 1);
  ovpp = mainw->vpp;
  mainw->vpp = vpp;

  // TODO - may need to restore palette, clamping and fps
  vppa = on_vpp_advanced_clicked(NULL, LIVES_INT_TO_POINTER(1));

  resp = lives_dialog_run(LIVES_DIALOG(vppa->dialog));

  if (resp == LIVES_RESPONSE_CANCEL) {
    error = TRUE;
    goto tr_err;
  }

  if (vpp->init_audio != NULL && mainw->save_with_sound && cfile->achans * cfile->arps > 0) {
    int in_arate = cfile->arps * cfile->arps / cfile->arate;
    if ((*vpp->init_audio)(in_arate, cfile->achans, mainw->vpp->extra_argc, mainw->vpp->extra_argv)) {
      audio = TRUE;
      spf = (double)(in_arate * cfile->achans * (cfile->asampsize >> 3)) / cfile->fps;

      afname = lives_build_filename(prefs->workdir, cfile->handle, CLIP_AUDIO_FILE, NULL);
      fd = lives_open_buffered_rdonly(afname);

      if (fd < 0) {
	do_read_failed_error_s(afname, lives_strerror(errno));
	error = TRUE;
	goto tr_err;
      }

      asigned = !(cfile->signed_endian & AFORM_UNSIGNED);
      aendian = cfile->signed_endian & AFORM_BIG_ENDIAN;
      
      if (cfile->asampsize > 8) {
	if ((aendian && (capable->byte_order == LIVES_BIG_ENDIAN)) || (!aendian && (capable->byte_order == LIVES_LITTLE_ENDIAN)))
	  swap_endian = TRUE;
	else swap_endian = FALSE;
      }

      abuff = lives_malloc((int)(spf + 1.) * cfile->achans * (cfile->asampsize >> 3));
      if (abuff == NULL) {
	error = TRUE;
	goto tr_err;
      }
      fltbuf = lives_malloc(cfile->achans * sizeof(float *));
      if (fltbuf == NULL) {
	error = TRUE;
	goto tr_err;
      }
    
      for (i = 0; i < cfile->achans; i++) {
	fltbuf[i] = (float *)lives_malloc((int)(spf + 1.) * cfile->achans * sizeof(float));
	if (fltbuf[i] == NULL) {
	  error = TRUE;
	  goto tr_err;
	}
	if (cfile->asampsize == 8) {
	  sbuff = (short *)lives_malloc((int)(spf + 1.) * cfile->achans * 2);
	}
      }
    }
  }
  
  (*vpp->init_screen)(cfile->hsize, cfile->vsize, FALSE, 0, vpp->extra_argc, vpp->extra_argv);

  frame_layer = weed_plant_new(WEED_PLANT_CHANNEL);
  weed_set_int_value(frame_layer, WEED_LEAF_CLIP, mainw->current_file);
  img_ext = get_image_ext_for_type(cfile->img_type);

  // encoding loop
  
  for (i = start; i <= end; i++) {
  // loop:
    weed_set_int_value(frame_layer, WEED_LEAF_FRAME, i);

    // - pull next frame (thread)
    pull_frame_threaded(frame_layer, img_ext, (weed_timecode_t)(currticks = lives_get_current_ticks(0, 0)));

    if (audio) {
      // - read 1 frame worth of audio, to float, send
      mainw->read_failed = FALSE;
      in_bytes = lives_read_buffered(fd, abuff, (size_t)spf * cfile->achans * (cfile->asampsize >> 3), TRUE);
      if (mainw->read_failed || in_bytes < 0) {
	error = TRUE;
	goto tr_err;
      }

      if (in_bytes == 0) {
	// eof, flush audio
	(*mainw->vpp->render_audio_frame_float)(NULL, 0);
      }
      else {
	nsamps = in_bytes / cfile->achans / (cfile->asampsize >> 3);
      
	// convert to float
	if (cfile->asampsize == 16) {
	  sample_move_d16_float(fltbuf[i], (short *)abuff, (uint64_t)nsamps, cfile->achans, asigned ? AFORM_SIGNED : AFORM_UNSIGNED, swap_endian, 1.0);
	}
	else {
	  sample_move_d8_d16(sbuff, (uint8_t *)abuff, (uint64_t)nsamps, in_bytes,
			     1.0, cfile->achans, cfile->achans, !asigned ? SWAP_U_TO_S : 0);
	  sample_move_d16_float(fltbuf[i], sbuff, (uint64_t)nsamps, cfile->achans, AFORM_SIGNED, FALSE, 1.0);
	}
	(*mainw->vpp->render_audio_frame_float)(fltbuf, nsamps);
      }
    }

    // get frame, send it
    check_layer_ready(frame_layer); // ensure all threads are complete
    convert_layer_palette(frame_layer, vpp->palette, vpp->YUV_clamping);

    // vid plugin expects compacted rowstrides (i.e. no padding/alignment after pixel row)
    compact_rowstrides(frame_layer);

    pd_array = weed_get_voidptr_array(frame_layer, WEED_LEAF_PIXEL_DATA, &weed_error);

    error = !(*mainw->vpp->render_frame)(weed_get_int_value(frame_layer, WEED_LEAF_WIDTH, &weed_error),
					 weed_get_int_value(mainw->frame_layer, WEED_LEAF_HEIGHT, &weed_error),
					 currticks, pd_array, NULL, NULL);

    lives_free(pd_array);
    weed_layer_pixel_data_free(frame_layer);

    if (error) goto tr_err;
  }


 tr_err:

  weed_layer_free(frame_layer);
  
  if (fd >= 0) lives_close_buffered(fd);

  lives_freep((void **)&afname);
  
  lives_freep((void **)&abuff);
  if (fltbuf != NULL) {
    for (i = 0; i < cfile->achans; lives_freep((void **)&(fltbuf[i++])));
    lives_free(fltbuf);
  }
  
  lives_freep((void **)&sbuff);
  
  if (vpp->exit_screen != NULL) {
    (*vpp->exit_screen)(0, 0);
  }

  // close vpp, unless mainw->vpp
  if (ovpp != NULL && vpp->handle != ovpp->handle) {
    close_vid_playback_plugin(vpp);
  }

  mainw->vpp = ovpp;

  return !error;
}

#endif
