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
  if (sfile->frame_index == NULL) return;

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
  char *fname, *fname_new;
  lives_clip_t *sfile = mainw->files[fileno];

  if (fileno == 0) return TRUE;

  if (sfile == NULL || sfile->frame_index == NULL) return FALSE;

  fname = lives_build_filename(prefs->workdir, sfile->handle, "file_index.back", NULL);
  fname_new = lives_build_filename(prefs->workdir, sfile->handle, "file_index", NULL);

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
      } else {
        if (sget_file_size(fname) != sfile->frames * 4) {
          retval = do_write_failed_error_s_with_retry(fname, NULL, NULL);
        } else {
          mainw->com_failed = FALSE;
          lives_cp(fname, fname_new);
          if (sget_file_size(fname_new) != sfile->frames * 4) {
            retval = do_write_failed_error_s_with_retry(fname, NULL, NULL);
          }
        }
      }
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(fname);
  lives_free(fname_new);

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

  return TRUE;
}


// load frame_index from disk
// returns -1 (error)
// or maxframe pointed to in clip

int load_frame_index(int fileno) {
  lives_clip_t *sfile = mainw->files[fileno];
  uint64_t filesize;
  char *fname;

  int fd;
  int retval;
  int maxframe = -1;

  register int i;

  if (sfile == NULL || sfile->frame_index != NULL) return -1;

  lives_freep((void **)&sfile->frame_index);

  fname = lives_build_filename(prefs->workdir, sfile->handle, "file_index", NULL);

  filesize = sget_file_size(fname);

  if (filesize <= 0) {
    lives_free(fname);
    return 0;
  }

  if (filesize >> 2 > sfile->frames) sfile->frames = filesize >> 2;

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
      LiVESResponseType response;
      char *what = lives_strdup(_("creating the frame index for the clip"));
      do {
        response = LIVES_RESPONSE_OK;
        create_frame_index(fileno, FALSE, 0, sfile->frames);
        if (cfile->frame_index == NULL) {
          response = do_memory_error_dialog(what, sfile->frames * 4);
        }
      } while (response == LIVES_RESPONSE_RETRY);
      lives_free(what);
      if (response == LIVES_RESPONSE_CANCEL) {
        break;
      }

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

  if (maxframe >= 0) sfile->clip_type = CLIP_TYPE_FILE;
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


static int scan_frames(lives_clip_t *sfile, int vframes, int last_real_frame) {
  register int i;
  for (i = 0; i < sfile->frames; i++) {
    // assume all real frames up to last_real_frame are there
    if ((sfile->frame_index[i] == -1 && i >= last_real_frame) || (sfile->frame_index[i] > vframes)) return i;
  }
  return i;
}


boolean check_clip_integrity(int fileno, const lives_clip_data_t *cdata, int maxframe) {
  lives_clip_t *sfile = mainw->files[fileno];

  lives_image_type_t empirical_img_type = sfile->img_type;

  int last_real_frame = sfile->frames;

  boolean has_missing_frames = FALSE;

  char *fname;

  register int i;

  // check clip integrity upon loading

  // check that cached values match with sfile (on disk) values
  // also check sfile->frame_index to make sure all frames are present

  // return FALSE if we find any omissions/inconsistencies

  // check the image type
  for (i = sfile->frames - 1; i >= 0; i--) {
    if (sfile->frame_index == NULL || sfile->frame_index[i] == -1) {
      // this is a non-virtual frame
      fname = make_image_file_name(sfile, i + 1, LIVES_FILE_EXT_PNG);
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        empirical_img_type = IMG_TYPE_PNG;
        lives_free(fname);
        break;
      }
      lives_free(fname);
      fname = make_image_file_name(sfile, i + 1, LIVES_FILE_EXT_JPG);
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        empirical_img_type = IMG_TYPE_JPEG;
        lives_free(fname);
        break;
      }
      lives_free(fname);
      last_real_frame = i;
      has_missing_frames = TRUE;
    }
  }

  if (cdata != NULL) {
    // check frame count
    if (maxframe > cdata->nframes || has_missing_frames) {
      has_missing_frames = TRUE;
      sfile->frames = scan_frames(sfile, cdata->nframes, last_real_frame);
    }
  }


  if (sfile->frame_index != NULL) {
    int lgoodframe = -1, goodidx;
    // check and attempt to correct frame_index
    for (i = 0; i < sfile->frames; i++) {
      int fr = sfile->frame_index[i];
      if (fr < -1 || (cdata == NULL && fr > sfile->frames - 1)
          || (cdata != NULL && fr > cdata->nframes - 1)) {
        has_missing_frames = TRUE;
        fname = make_image_file_name(sfile, i + 1, LIVES_FILE_EXT_PNG);
        if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) sfile->frame_index[i] = -1;
        else {
          if (lgoodframe != -1) {
            sfile->frame_index[i] = lgoodframe + i - goodidx;
          } else {
            sfile->frame_index[i] = i;
          }
          if (sfile->frame_index[i] >= cdata->nframes) sfile->frame_index[i] = cdata->nframes - 1;
        }
        lives_free(fname);
      } else {
        if (cdata != NULL && fr != -1) {
          lgoodframe = fr;
          goodidx = i;
        }
      }
    }
  }

  if (sfile->frames > 0) {
    int hsize = sfile->hsize;
    int vsize = sfile->vsize;

    if (last_real_frame > 0) {
      sfile->img_type = empirical_img_type;
      get_frames_sizes(fileno, last_real_frame);
    } else {
      if (cdata != NULL) {
        if (!prefs->auto_nobord) {
          sfile->hsize = cdata->frame_width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
          sfile->vsize = cdata->frame_height;
        } else {
          sfile->hsize = cdata->width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
          sfile->vsize = cdata->height;
        }
      }
    }

    if (sfile->hsize != hsize || sfile->vsize != vsize) goto mismatch;
  }

  if (has_missing_frames) goto mismatch;

  if (cdata != NULL && (fabs(sfile->fps - (double)cdata->fps) > prefs->fps_tolerance)) goto mismatch;

  if (sfile->img_type != empirical_img_type) sfile->img_type = empirical_img_type;

  /*
    // ignore since we may have resampled audio
    if (sfile->achans != cdata->achans || sfile->arps != cdata->arate || sfile->asampsize != cdata->asamps ||
      cdata->asigned == (sfile->signed_endian & AFORM_UNSIGNED)) return FALSE;
  */

  // all things equal as far as we can tell
  return TRUE;

mismatch:
  // something mismatched - trust the disk version
  if (cdata != NULL)((lives_clip_data_t *)cdata)->fps = sfile->pb_fps = sfile->fps;

  sfile->img_type = empirical_img_type;

  sfile->needs_update = TRUE;

  sfile->afilesize = reget_afilesize_inner(fileno);

  return FALSE;
}


boolean check_if_all_virtual(int fileno, int start, int end) {
  // check if all frames are virtual from start to end inclusive in clip fileno
  register int i;
  lives_clip_t *sfile = mainw->files[fileno];

  if (sfile->frame_index == NULL) return FALSE;
  for (i = start; i <= end; i++) {
    if (sfile->frame_index[i - 1] == -1) return FALSE;
  }

  return TRUE;
}


boolean check_if_non_virtual(int fileno, int start, int end) {
  // check if there are no virtual frames from start to end inclusive in clip fileno

  // return FALSE if any virtual frames are found in the region
  // return TRUE if all frames in region are non-virtual

  // also may change the clip_type and the interlace

  register int i;
  lives_clip_t *sfile = mainw->files[fileno];
  char *ppath, *cwd;
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

  cwd = lives_get_current_dir();
  ppath = lives_build_filename(prefs->workdir, sfile->handle, NULL);
  lives_chdir(ppath, FALSE);
  lives_free(ppath);
  close_decoder_plugin((lives_decoder_t *)sfile->ext_src);
  sfile->ext_src = NULL;
  lives_chdir(cwd, FALSE);
  lives_free(cwd);

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

#define DS_SPACE_CHECK_FRAMES 100


static boolean save_decoded(int fileno, int i, LiVESPixbuf *pixbuf, boolean silent, int progress) {
  lives_clip_t *sfile = mainw->files[fileno];
  boolean retb;
  int retval;
  LiVESError *error = NULL;
  char *oname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));

  do {
    retval = LIVES_RESPONSE_NONE;
    retb = lives_pixbuf_save(pixbuf, oname, sfile->img_type, 100 - prefs->ocp, &error);
    if (error != NULL && !silent) {
      retval = do_write_failed_error_s_with_retry(oname, error->message, NULL);
      lives_error_free(error);
      error = NULL;
    } else if (!retb) {
      retval = do_write_failed_error_s_with_retry(oname, NULL, NULL);
    }
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_freep((void **)&oname);

  if (progress % DS_SPACE_CHECK_FRAMES == 1) {
    if (!check_storage_space(fileno, FALSE)) {
      retval = LIVES_RESPONSE_CANCEL;
    }
  }

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;
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

  int progress = 1;

  if (sframe < 1) sframe = 1;

  for (i = sframe; i <= eframe; i++) {
    if (i > sfile->frames) break;

    if (update_progress) {
      threaded_dialog_spin((double)(i - sframe) / (double)(eframe - sframe + 1));
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
      lives_widget_context_update();
    }

    if (sfile->frame_index[i - 1] >= 0) {
      if (pbr != NULL && pixbuf != NULL) lives_widget_object_unref(pixbuf);

      pixbuf = pull_lives_pixbuf_at_size(sfileno, i, get_image_ext_for_type(sfile->img_type),
                                         q_gint64((i - 1.) / sfile->fps, sfile->fps), sfile->hsize, sfile->vsize, LIVES_INTERP_BEST);

      if (pixbuf == NULL) return FALSE;

      if (!save_decoded(sfileno, i, pixbuf, pbr != NULL, progress)) return FALSE;

      if (pbr == NULL) {
        if (pixbuf != NULL) lives_widget_object_unref(pixbuf);
        pixbuf = NULL;
      }

      // another thread may have called check_if_non_virtual - TODO : use a mutex
      if (sfile->frame_index == NULL) break;
      sfile->frame_index[i - 1] = -1;

      if (update_progress) {
        // sig_progress...
        lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%d", progress++);
        threaded_dialog_spin((double)(i - sframe) / (double)(eframe - sframe + 1));
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


static void restore_gamma_cb(int gamma_type) {
  // experimental, will move to another file
  lives_clip_t *ocb = clipboard, *ncb;
  LiVESPixbuf *pixbuf;
  int cf = mainw->current_file;
  int ogamma;
  int i, found = -1, progress = 0;
  boolean is_stored = FALSE;

  for (i = 0; i < mainw->ncbstores; i++) {
    if (mainw->cbstores[i] == clipboard) is_stored = TRUE;
    if (mainw->cbstores[i]->gamma_type == cfile->gamma_type) found = i;
    if (found > -1 && is_stored) break;
  }
  if (!is_stored) {
    if (mainw->ncbstores >= MAX_CBSTORES) return;
    mainw->cbstores[mainw->ncbstores++] = clipboard;
  }
  if (found > -1) {
    clipboard = mainw->cbstores[found];
    return;
  }
  // not found,
  // we'll actually make a copy of the clipboard but with the new gamma_type
  clipboard = NULL;
  init_clipboard();
  lives_memcpy(clipboard, ocb, sizeof(lives_clip_t));
  lives_memcpy(clipboard->frame_index, ocb->frame_index, clipboard->frames * 4);
  ncb = clipboard;
  ogamma = ocb->gamma_type;
  ocb->gamma_type = ncb->gamma_type = gamma_type; // force conversion
  for (i = 0; i < clipboard->frames; i++) {
    if (clipboard->frame_index[i] == -1) {
      progress++;
      clipboard = ocb;
      pixbuf = pull_lives_pixbuf(0, i, get_image_ext_for_type(ocb->img_type), 0);
      clipboard = ncb;
      if (!save_decoded(0, i, pixbuf, FALSE, progress)) {
        mainw->current_file = 0;
        close_current_file(cf);
        clipboard = ocb;
        ocb->gamma_type = ogamma;
        return;
      }
    }
  }
  ocb->gamma_type = ogamma;
}


boolean realize_all_frames(int clipno, const char *msg, boolean enough) {
  // if enough is set, we show Enough button instead of Cancel.
  int current_file = mainw->current_file;
  mainw->cancelled = CANCEL_NONE;
  if (!IS_VALID_CLIP(clipno)) return FALSE;

  // if its the clipboard and we have exotic gamma types we need to do a special thing
  // - fix the gamma_type of the clipboard existing frames before inserting in cfile
  if (clipno == 0 && prefs->btgamma && CURRENT_CLIP_HAS_VIDEO && cfile->gamma_type != clipboard->gamma_type) {
    restore_gamma_cb(cfile->gamma_type);
  }

  if (!check_if_non_virtual(clipno, 1, mainw->files[clipno]->frames)) {
    boolean retb;
    mainw->current_file = clipno;
    cfile->progress_start = 1;
    cfile->progress_end = count_virtual_frames(cfile->frame_index, 1, cfile->frames);
    if (enough) mainw->cancel_type = CANCEL_SOFT; // force "Enough" button to be shown
    do_threaded_dialog((char *)msg, TRUE);
    lives_widget_show_all(mainw->files[clipno]->proc_ptr->processing);
    mainw->cancel_type = CANCEL_KILL;
    retb = virtual_to_images(mainw->current_file, 1, cfile->frames, TRUE, NULL);
    end_threaded_dialog();
    mainw->current_file = current_file;

    if (mainw->cancelled != CANCEL_NONE || !retb) {
      mainw->cancelled = CANCEL_USER;
      return FALSE;
    }
  }
  return TRUE;
}


void insert_images_in_virtual(int sfileno, int where, int frames, int *frame_index, int start) {
  // insert physical (frames) images (or virtual possibly) into sfile at position where [0 = before first frame]
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESResponseType response;

  char *what;
  int nframes = sfile->frames;

  register int i, j = start - 1;

  if (sfile->frame_index == NULL) return;

  what = lives_strdup(_("creating the new frame index for the clip"));
  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  do {
    response = LIVES_RESPONSE_OK;
    create_frame_index(sfileno, FALSE, 0, nframes + frames);
    if (sfile->frame_index == NULL) {
      response = do_memory_error_dialog(what, (nframes + frames) * 4);
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    return;
  }

  lives_memcpy(sfile->frame_index, sfile->frame_index_back, where * 4);

  for (i = where; i < where + frames; i++) {
    if (frame_index != NULL && frame_index[j] != -1) sfile->frame_index[i] = frame_index[j];
    else sfile->frame_index[i] = -1;
    if (++j >= sfile->frames) j = 0;
  }

  lives_memcpy(&sfile->frame_index[where + frames], &sfile->frame_index_back[where], (nframes - where) * 4);

  sfile->frames += frames;
  save_frame_index(sfileno);
  sfile->frames -= frames;
}


void delete_frames_from_virtual(int sfileno, int start, int end) {
  // delete (frames) images from sfile at position start to end
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESResponseType response;

  char *what = lives_strdup(_("creating the new frame index for the clip"));
  int nframes = sfile->frames, frames = end - start + 1;

  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  if (nframes - frames == 0) {
    del_frame_index(sfile);
    return;
  }

  do {
    response = LIVES_RESPONSE_OK;
    create_frame_index(sfileno, FALSE, 0, nframes - frames);
    if (sfile->frame_index == NULL) {
      response = do_memory_error_dialog(what, (nframes - frames) * 4);
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    return;
  }

  lives_memcpy(sfile->frame_index, sfile->frame_index_back, start * 4);
  lives_memcpy(&sfile->frame_index[end - frames], &sfile->frame_index_back[end], (nframes - end) * 4);

  sfile->frames = nframes - frames;
  save_frame_index(sfileno);
  sfile->frames = nframes;
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
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
    threaded_dialog_spin(0.);

    if ((i <= sfile->frames && sfile->frame_index[i - 1] != -1) || i > sfile->frames) {
      iname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      lives_rm(iname);
    }
  }
}


int *frame_index_copy(int *findex, int nframes, int offset) {
  // copy first nframes from findex and return them, adding offset to each value
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
  if (!IS_VALID_CLIP(sfileno)) return FALSE;
  if (sfile->frame_index == NULL) return FALSE;
  if (frame < 1 || frame > sfile->frames) return FALSE;
  if (sfile->frame_index[frame - 1] != -1) return TRUE;
  return FALSE;
}


void insert_blank_frames(int sfileno, int nframes, int after, int palette) {
  // insert blank frames in clip (only valid just after clip is opened)

  // this is ugly, it should be moved to another file

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESPixbuf *blankp = NULL;
  LiVESError *error = NULL;
  char oname[PATH_MAX];
  char nname[PATH_MAX];
  char *tmp;

  register int i;

  if (!check_if_all_virtual(sfileno, 1, sfile->frames)) {
    for (i = after + 1; i <= sfile->frames; i++) {
      if (sfile->frame_index == NULL || sfile->frame_index[i - 1] == -1) {
        tmp = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
        lives_snprintf(oname, PATH_MAX, "%s", tmp);
        lives_free(tmp);
        if (lives_file_test(oname, LIVES_FILE_TEST_EXISTS)) {
          tmp = make_image_file_name(sfile, i + nframes, get_image_ext_for_type(sfile->img_type));
          lives_snprintf(nname, PATH_MAX, "%s", tmp);
          lives_free(tmp);
          mainw->com_failed = FALSE;
          lives_mv(oname, nname);
          if (mainw->com_failed) {
            return;
          }
        }
      }
    }
  }

  for (i = after; i < after + nframes; i++) {
    tmp = make_image_file_name(sfile, i + 1, get_image_ext_for_type(sfile->img_type));
    lives_snprintf(oname, PATH_MAX, "%s", tmp);
    lives_free(tmp);
    if (blankp == NULL) blankp = lives_pixbuf_new_blank(sfile->hsize, sfile->vsize, palette);
    lives_pixbuf_save(blankp, oname, sfile->img_type, 100 - prefs->ocp, &error);
    if (error != NULL) {
      char *msg = lives_strdup_printf(_("Padding: Unable to write blank frame with size %d x %d to %s"),
                                      sfile->hsize, sfile->vsize, oname);
      LIVES_ERROR(msg);
      lives_free(msg);
      lives_error_free(error);
      break;
    }
  }

  nframes = i - after; // in case we bailed

  if (sfile->clip_type == CLIP_TYPE_FILE)
    insert_images_in_virtual(sfileno, after, nframes, NULL, 0);

  sfile->frames += nframes;

  if (blankp != NULL) lives_widget_object_unref(blankp);
}


boolean pull_frame_idle(livespointer data) {
  if (cfile->fx_frame_pump >= cfile->end) return FALSE;
  if (!virtual_to_images(mainw->current_file, cfile->fx_frame_pump, cfile->fx_frame_pump, FALSE, NULL)) {
    return FALSE;
  }
  cfile->fx_frame_pump++;
  return TRUE;
}
