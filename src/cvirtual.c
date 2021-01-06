// cvirtual.c
// LiVES
// (c) G. Finch 2008 - 2020 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling "virtual" clips (CLIP_TYPE_FILE)

#include "main.h"

// frame_index is for files of type CLIP_TYPE_FILE
// a positive number is a pointer to a frame within the video file
// -1 means frame is stored as the corresponding image file
// e.g 00000001.jpg or 00000010.png etc.

#include "resample.h"
#include "cvirtual.h"

/** count virtual frames between start and end (inclusive) */
frames_t count_virtual_frames(frames_t *findex, frames_t start, frames_t end) {
  frames_t count = 0;
  for (register int i = start - 1; i < end; i++) if (findex[i] != -1) count++;
  return count;
}


boolean create_frame_index(int fileno, boolean init, frames_t start_offset, frames_t nframes) {
  lives_clip_t *sfile = mainw->files[fileno];
  size_t idxsize = (ALIGN_CEIL(nframes * sizeof(frames_t), DEF_ALIGN)) / DEF_ALIGN;
  if (!IS_VALID_CLIP(fileno) || sfile->frame_index) return FALSE;
  sfile->frame_index = (frames_t *)lives_calloc(idxsize, DEF_ALIGN);
  if (!sfile->frame_index) return FALSE;
  if (init) for (int i = 0; i < sfile->frames; i++) sfile->frame_index[i] = i + start_offset;
  return TRUE;
}


static boolean extend_frame_index(int fileno, frames_t start, frames_t end) {
  lives_clip_t *sfile = mainw->files[fileno];
  size_t idxsize = (ALIGN_CEIL(end * sizeof(frames_t), DEF_ALIGN)) / DEF_ALIGN;
  if (!IS_VALID_CLIP(fileno) || start > end) return FALSE;
  if (sfile->frame_index_back) lives_free(sfile->frame_index_back);
  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = (frames_t *)lives_calloc(idxsize, DEF_ALIGN);
  if (!sfile->frame_index) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    return FALSE;
  }
  for (int i = start; i < end; i++) sfile->frame_index[i] = -1;
  return TRUE;
}


// save frame_index to disk
boolean save_frame_index(int fileno) {
  int fd, i;
  int retval;
  char *fname, *fname_new;
  lives_clip_t *sfile = mainw->files[fileno];

  if (fileno == 0) return TRUE;

  if (!sfile || !sfile->frame_index) return FALSE;

  fname = lives_build_filename(prefs->workdir, sfile->handle, FRAME_INDEX_FNAME "." LIVES_FILE_EXT_BACK, NULL);
  fname_new = lives_build_filename(prefs->workdir, sfile->handle, FRAME_INDEX_FNAME, NULL);

  do {
    retval = 0;
    fd = lives_create_buffered(fname, DEF_FILE_PERMS);
    if (fd < 0) {
      retval = do_write_failed_error_s_with_retry(fname, lives_strerror(errno));
    } else {
      for (i = 0; i < sfile->frames; i++) {
        lives_write_le_buffered(fd, &sfile->frame_index[i], sizeof(frames_t), TRUE);
        if (THREADVAR(write_failed) == fd + 1) {
          THREADVAR(write_failed) = 0;
          break;
        }
      }

      lives_close_buffered(fd);

      if (mainw->is_exiting) return TRUE;

      if (THREADVAR(write_failed) == fd + 1) {
        THREADVAR(write_failed) = 0;
        retval = do_write_failed_error_s_with_retry(fname, NULL);
      } else {
        if (sget_file_size(fname) != (off_t)sfile->frames * sizeof(frames_t)) {
          retval = do_write_failed_error_s_with_retry(fname, NULL);
        } else {
          lives_cp(fname, fname_new);
          if (sget_file_size(fname_new) != (off_t)sfile->frames * sizeof(frames_t)) {
            retval = do_write_failed_error_s_with_retry(fname, NULL);
	    // *INDENT-OFF*
          }}}}
    // *INDENT-ON*
  } while (retval == LIVES_RESPONSE_RETRY);

  lives_free(fname);
  lives_free(fname_new);

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

  return TRUE;
}

// load frame_index from disk
// returns -1 (error)
// or maxframe pointed to in clip

frames_t load_frame_index(int fileno) {
  lives_clip_t *sfile = mainw->files[fileno];
  off_t filesize;
  char *fname, *fname_back;
  boolean backuptried = FALSE;
  int fd, retval;
  frames_t maxframe = -1;

  int i;

  if (!sfile || sfile->frame_index) return -1;

  lives_freep((void **)&sfile->frame_index);

  fname = lives_build_filename(prefs->workdir, sfile->handle, FRAME_INDEX_FNAME, NULL);
  filesize = sget_file_size(fname);

  if (filesize <= 0) {
    lives_free(fname);
    return 0;
  }

  if (filesize >> 2 > (off_t)sfile->frames) sfile->frames = (frames_t)(filesize >> 2);
  fname_back = lives_build_filename(prefs->workdir, sfile->handle, FRAME_INDEX_FNAME "." LIVES_FILE_EXT_BACK, NULL);

  do {
    retval = 0;
    fd = lives_open_buffered_rdonly(fname);
    if (fd < 0) {
      THREADVAR(read_failed) = 0;
      retval = do_read_failed_error_s_with_retry(fname, lives_strerror(errno));
      if (!backuptried) {
        fd = lives_open_buffered_rdonly(fname_back);
        if (fd >= 0) {
          lives_close_buffered(fd);
          if (findex_bk_dialog(fname_back)) {
            lives_cp(fname_back, fname);
            backuptried = TRUE;
            continue;
          }
        }
      } else if (sfile->frame_index_back) {
        if (findex_bk_dialog(fname_back)) {
          sfile->frame_index = sfile->frame_index_back;
        }
      }
      if (retval == LIVES_RESPONSE_CANCEL) {
        lives_free(fname);
        lives_free(fname_back);
        return -1;
      }
    } else {
      LiVESResponseType response;
      char *what = (_("creating the frame index for the clip"));
      do {
        response = LIVES_RESPONSE_OK;
        create_frame_index(fileno, FALSE, 0, sfile->frames);
        if (!cfile->frame_index) {
          response = do_memory_error_dialog(what, sfile->frames * sizeof(frames_t));
        }
      } while (response == LIVES_RESPONSE_RETRY);
      lives_free(what);
      if (response == LIVES_RESPONSE_CANCEL) {
        break;
      }

      for (i = 0; i < sfile->frames; i++) {
        lives_read_le_buffered(fd, &sfile->frame_index[i], sizeof(frames_t), FALSE);
        if (THREADVAR(read_failed) == fd + 1) {
          break;
        }
        if (sfile->frame_index[i] > maxframe) {
          maxframe = sfile->frame_index[i];
        }
      }
      lives_close_buffered(fd);

      if (THREADVAR(read_failed) == fd + 1) {
        THREADVAR(read_failed) = 0;
        retval = do_read_failed_error_s_with_retry(fname, NULL);
      }

      if (!backuptried) {
        backuptried = TRUE;
        fd = lives_open_buffered_rdonly(fname_back);
        if (fd >= 0) {
          LiVESList *list = NULL;
          frames_t vframe;
          int count = 0;
          for (; lives_read_le_buffered(fd, &vframe, sizeof(frames_t), TRUE) == sizeof(frames_t); count++) {
            if (THREADVAR(read_failed) == fd + 1) break;
            list = lives_list_prepend(list, LIVES_INT_TO_POINTER(vframe));
          }
          lives_close_buffered(fd);
          if (THREADVAR(read_failed) == fd + 1) {
            THREADVAR(read_failed) = 0;
          } else if (count) {
            frames_t *f_index = sfile->frame_index;
            list = lives_list_reverse(list);
            lives_freep((void **)&sfile->frame_index_back);
            sfile->frame_index = NULL;
            create_frame_index(fileno, FALSE, 0, count);
            sfile->frame_index_back = sfile->frame_index;
            sfile->frame_index = f_index;
            if (sfile->frame_index_back) {
              LiVESList *xlist;
              sfile->old_frames = count;
              count = 0;
              for (xlist = list; xlist; xlist = xlist->next) {
                sfile->frame_index_back[count++] = LIVES_POINTER_TO_INT(xlist->data);
		// *INDENT-OFF*
	      }}}
	  if (list) lives_list_free(list);
	}}}} while (retval == LIVES_RESPONSE_RETRY);
  // *INDENT-ON*

  lives_free(fname);
  lives_free(fname_back);

  if (maxframe >= 0) sfile->clip_type = CLIP_TYPE_FILE;
  return ++maxframe;
}


void del_frame_index(lives_clip_t *sfile) {
  // physically delete the frame_index for a clip
  // only done once all

  char *idxfile;

  // cannot call check_if_non_virtual() else we end up recursing

  if (sfile->frame_index) {
    for (frames_t i = 1; i <= sfile->frames; i++) {
      if (sfile->frame_index[i - 1] != -1) {
        LIVES_ERROR("deleting frame_index with virtual frames in it !");
        return;
      }
    }
  }

  if (sfile != clipboard) {
    idxfile = lives_build_filename(prefs->workdir, sfile->handle, FRAME_INDEX_FNAME, NULL);
    lives_rm(idxfile);
    lives_free(idxfile);
  }

  lives_freep((void **)&sfile->frame_index);
}


static frames_t scan_frames(lives_clip_t *sfile, frames_t vframes, frames_t last_real_frame) {
  frames_t i;
  for (i = 0; i < sfile->frames; i++) {
    // assume all real frames up to last_real_frame are there
    if ((sfile->frame_index[i] == -1 && i >= last_real_frame) || (sfile->frame_index[i] > vframes)) return i;
  }
  return i;
}


lives_img_type_t resolve_img_type(lives_clip_t *sfile) {
  lives_img_type_t ximgtype;
  int nimty = (int)N_IMG_TYPES;
  char *fname;
  for (frames_t i = sfile->frames - 1; i >= 0; i--) {
    if (!sfile->frame_index || sfile->frame_index[i] == -1) {
      for (int j = 1; j < nimty; j++) {
        ximgtype = (lives_img_type_t)j;
        fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(ximgtype));
        if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
          lives_free(fname);
          return j;
        }
        lives_free(fname);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  return IMG_TYPE_BEST;
}


boolean check_clip_integrity(int fileno, const lives_clip_data_t *cdata, frames_t maxframe) {
  lives_clip_t *sfile = mainw->files[fileno], *binf = NULL;
  lives_img_type_t empirical_img_type = sfile->img_type, oemp = empirical_img_type;
  lives_img_type_t ximgtype;
  frames_t last_real_frame = sfile->frames;
  int nimty = (int)N_IMG_TYPES, j;
  boolean has_missing_frames = FALSE, bad_imgfmts = FALSE;
  boolean mismatch = FALSE;
  boolean isfirst = TRUE;
  boolean backup_more_correct = FALSE;
  char *fname;

  frames_t i;
  // check clip integrity upon loading

  // check that cached values match with sfile (on disk) values
  // also check sfile->frame_index to make sure all frames are present

  // return FALSE if we find any omissions/inconsistencies

  /* if (sfile->frames > maxframe) { */
  /*   has_missing_frames = TRUE; */
  /*   sfile->frames = maxframe; */
  /* } */

  if (prefs->vj_mode) return TRUE;

  // check the image type
  for (i = sfile->frames - 1; i >= 0; i--) {
    if (!sfile->frame_index || sfile->frame_index[i] == -1) {
      // this is a non-virtual frame
      ximgtype = empirical_img_type;
      fname = NULL;
      if (ximgtype != IMG_TYPE_UNKNOWN) fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(ximgtype));
      if (!fname || !lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        if (fname) lives_free(fname);
        for (j = 1; j < nimty; j++) {
          ximgtype = (lives_img_type_t)j;
          if (ximgtype == empirical_img_type) continue;
          fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(ximgtype));
          if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
            if (isfirst) {
              empirical_img_type = ximgtype;
              if (oemp == IMG_TYPE_UNKNOWN) oemp = ximgtype;
            } else {
              if (ximgtype == oemp) empirical_img_type = oemp;
              bad_imgfmts = TRUE;
            }
            lives_free(fname);
            isfirst = FALSE;
            break;
          }
          lives_free(fname);
        }
        if (j == nimty) {
          has_missing_frames = TRUE;
          if (prefs->show_dev_opts) {
            g_printerr("clip %s is missing image frame %d\n", sfile->handle, i + 1);
          }
        } else {
          last_real_frame = i;
          isfirst = FALSE;
	  // *INDENT-OFF*
	}}
      else lives_free(fname);
    }}
  // *INDENT-ON*

  if (empirical_img_type == IMG_TYPE_UNKNOWN) {
    /// this is possible if clip is only virtual frames
    empirical_img_type = sfile->img_type = IMG_TYPE_BEST; // read_headers() will have set this to "jpeg" (default)
  }

  if (cdata) {
    // check frame count
    if (maxframe > cdata->nframes || has_missing_frames) {
      if (prefs->show_dev_opts) {
        if (maxframe > cdata->nframes) {
          g_printerr("frame count mismatch for clip %d,  %s, maxframe is %d, decoder claims only %ld\nRescaning...",
                     fileno, sfile->handle, maxframe, cdata-> nframes);
        }
      }

      has_missing_frames = TRUE;
      sfile->frames = scan_frames(sfile, cdata->nframes, last_real_frame);
      if (prefs->show_dev_opts) {
        g_printerr("rescan counted %d frames\n.", sfile->frames);
      }
    }
  }

  if (sfile->frame_index) {
    frames_t lgoodframe = -1;
    int goodidx;
    frames_t xframes = sfile->frames;

    if (sfile->frame_index_back) {
      if (sfile->old_frames > sfile->frames) xframes = sfile->old_frames;
      backup_more_correct = TRUE;
    }

    // check and attempt to correct frame_index
    for (i = 0; i < xframes; i++) {
      frames_t fr;
      if (i < sfile->frames) fr = sfile->frame_index[i];
      else fr = sfile->frame_index_back[i];
      if (fr < -1 || (!cdata && (frames64_t)fr > sfile->frames - 1)
          || (cdata && (frames64_t)fr > cdata->nframes - 1)) {
        if (i >= sfile->frames) {
          backup_more_correct = FALSE;
          break;
        }
        if (backup_more_correct && i < sfile->old_frames) {
          frames_t fr2 = sfile->frame_index_back[i];
          if (fr2 < -1 || (!cdata && (frames64_t)fr2 > sfile->frames - 1)
              || (cdata && (frames64_t)fr2 > cdata->nframes - 1)) {
            backup_more_correct = FALSE;
          }
        }

        if (prefs->show_dev_opts) {
          g_printerr("bad frame index %d, points to %d.....", i, fr);
        }
        if (fr < sfile->frames) has_missing_frames = TRUE;
        fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(empirical_img_type));
        if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
          sfile->frame_index[i] = -1;
          if (prefs->show_dev_opts) {
            g_printerr("relinked to image frame %d\n", i + 1);
          }
        } else {
          if (backup_more_correct && i < sfile->old_frames && sfile->frame_index_back[i] == -1)
            backup_more_correct = FALSE;
          if (lgoodframe != -1) {
            sfile->frame_index[i] = lgoodframe + i - goodidx;
            if (prefs->show_dev_opts) {
              g_printerr("relinked to clip frame %d\n", lgoodframe + i - goodidx);
            }
          } else {
            sfile->frame_index[i] = i;
            if (prefs->show_dev_opts) {
              g_printerr("reset to clip frame %d\n", i);
            }
          }
          if (sfile->frame_index[i] >= cdata->nframes) sfile->frame_index[i] = cdata->nframes - 1;
        }
        lives_free(fname);
      } else {
        if (cdata && fr != -1) {
          lgoodframe = fr;
          goodidx = i;
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
  if (has_missing_frames && backup_more_correct) {
    lives_freep((void **)&sfile->frame_index);
    sfile->frame_index = sfile->frame_index_back;
    if (sfile->old_frames > sfile->frames) {
      sfile->frames = sfile->old_frames;
      sfile->frames = scan_frames(sfile, sfile->frames, last_real_frame);
    } else sfile->frames = sfile->old_frames;
  }

  if (sfile->frames > 0) {
    int hsize = sfile->hsize, chsize = hsize;
    int vsize = sfile->vsize, cvsize = vsize;
    frames_t last_img_frame = -1;
    if (last_real_frame > 0) {
      if (sfile->frame_index) {
        for (i = last_real_frame; i > 0; i--) {
          if (sfile->frame_index[i - 1] == -1) {
            last_img_frame = i;
            break;
          }
        }
      } else last_img_frame = last_real_frame;
      if (last_img_frame > -1) {
        sfile->img_type = empirical_img_type;
        get_frames_sizes(fileno, last_img_frame, &hsize, &vsize);
      }
    }

    if (cdata) {
      if (!prefs->auto_nobord) {
        chsize = cdata->frame_width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
        cvsize = cdata->frame_height;
      } else {
        chsize = cdata->width * weed_palette_get_pixels_per_macropixel(cdata->current_palette);
        cvsize = cdata->height;
      }
    }

    if (chsize == hsize && hsize != sfile->hsize) sfile->hsize = hsize;
    if (cvsize == vsize && vsize != sfile->vsize) sfile->vsize = vsize;

    if (last_real_frame > 0) {
      if (hsize == sfile->hsize && vsize == sfile->vsize) {
        frames_t fframe = 0;
        /// last frame is most likely to return correct size
        /// we should also check first frame, as it is more likely to be wrong
        if (sfile->clip_type == CLIP_TYPE_DISK) fframe = 1;
        else {
          for (i = 1; i < last_real_frame; i++) {
            if (sfile->frame_index[i - 1] == -1) {
              fname = make_image_file_name(sfile, i, get_image_ext_for_type(empirical_img_type));
              if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
                fframe = i;
                lives_free(fname);
                break;
              }
              has_missing_frames = TRUE;
              lives_free(fname);
	      // *INDENT-OFF*
	    }}}
	if (fframe) get_frames_sizes(fileno, fframe, &hsize, &vsize);
      }}
    // *INDENT-ON*

    if (sfile->hsize != hsize || sfile->vsize != vsize) {
      LiVESResponseType resp = do_resize_dlg(sfile->hsize, sfile->vsize, hsize, vsize);
      if (prefs->show_dev_opts) {
        g_printerr("incorrect frame size %d X %d, corrected to %d X %d\n", hsize, vsize, sfile->hsize, sfile->vsize);
      }
      if (resp == LIVES_RESPONSE_ACCEPT) {
        sfile->hsize = hsize;
        sfile->vsize = vsize;
      } else if (resp == LIVES_RESPONSE_YES) {
        int missing = 0, nbadsized = 0;
        threaded_dialog_push();
        do_threaded_dialog(_("Resizing all frames\n"), TRUE);
        lives_widget_show_all(mainw->proc_ptr->processing);
        if (resize_all(fileno, sfile->hsize, sfile->vsize, empirical_img_type, FALSE,
                       &nbadsized, &missing)) {
          g_printerr("resize detected %d bad sized, %d missing \n", nbadsized, missing);
          if (missing) has_missing_frames = TRUE;
          if (mainw->cancelled == CANCEL_NONE) bad_imgfmts = FALSE;
          else mismatch = TRUE;
        }
        mainw->cancelled = CANCEL_NONE;
        end_threaded_dialog();
        threaded_dialog_pop();
      } else mismatch = TRUE;
    }
  }
  if (bad_imgfmts) {
    LiVESResponseType resp = do_imgfmts_error(empirical_img_type);
    if (resp == LIVES_RESPONSE_OK) {
      int missing = 0, nbadsized = 0;
      threaded_dialog_push();
      do_threaded_dialog(_("Correcting Image Formats\n"), TRUE);
      if (resize_all(fileno, sfile->hsize, sfile->vsize, empirical_img_type, FALSE,
                     &nbadsized, &missing)) {
        g_printerr("change fmts detected %d bad sized, %d missing \n", nbadsized, missing);
        if (missing) has_missing_frames = TRUE;
        if (mainw->cancelled == CANCEL_NONE) bad_imgfmts = FALSE;
        else mismatch = TRUE;
      }
      mainw->cancelled = CANCEL_NONE;
      end_threaded_dialog();
      threaded_dialog_pop();
    } else mismatch = TRUE;
    mainw->cancelled = CANCEL_NONE;
  }

  if (has_missing_frames) mismatch = TRUE;
  else {
    if (sfile->frame_index) {
      for (i = 0; i < sfile->frames; i++) {
        if (sfile->frame_index[i] != -1) {
          fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(empirical_img_type));
          if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
            lives_rm(fname);
	    // *INDENT-OFF*
	  }
	  lives_free(fname);
	}}}}
  // *INDENT-ON*
  if (cdata && (fabs(sfile->fps - (double)cdata->fps) > prefs->fps_tolerance)) {
    if (prefs->show_dev_opts) {
      g_printerr("fps mismtach, claimed %f, cdata said %f\n", sfile->fps, cdata->fps);
    }
    mismatch = TRUE;
  }
  if (sfile->img_type != empirical_img_type) {
    if (prefs->show_dev_opts) {
      g_printerr("corrected image type from %d to %d\n", sfile->img_type, empirical_img_type);
    }
    sfile->img_type = empirical_img_type;
  }

  if (mismatch) goto mismatch;

  /*
    // ignore since we may have resampled audio
    if (sfile->achans != cdata->achans || sfile->arps != cdata->arate || sfile->asampsize != cdata->asamps ||
      cdata->asigned == (sfile->signed_endian & AFORM_UNSIGNED)) return FALSE;
  */

  // all things equal as far as we can tell
  return TRUE;

mismatch:
  // something mismatched - commence further investigation

  if ((binf = clip_forensic(mainw->current_file))) {
    if (has_missing_frames) {
      if (cdata) {
        if (binf->frames == cdata->nframes  && binf->frames < sfile->frames) sfile->frames = binf->frames;
        else if (binf->frames == sfile->frames && binf->frames < sfile->frames) {
          if (sfile->frames > cdata->nframes) if (!extend_frame_index(fileno, cdata->nframes, sfile->frames)) return FALSE;
          ((lives_clip_data_t *)cdata)->nframes = sfile->frames;
        }
      } else if (binf->frames <= sfile->frames) sfile->frames = binf->frames;
    }
    if (cdata && binf->fps == cdata->fps)  {
      ((lives_clip_data_t *)cdata)->fps =  sfile->pb_fps = sfile->fps;
    }
  }

  sfile->img_type = empirical_img_type;

  sfile->needs_update = TRUE;

  sfile->afilesize = reget_afilesize_inner(fileno);

  if (has_missing_frames && sfile->frame_index) {
    if (sfile->frames > maxframe) extend_frame_index(fileno, maxframe, sfile->frames);
    save_frame_index(fileno);
  }
  return FALSE;
}


frames_t first_virtual_frame(int fileno, frames_t start, frames_t end) {
  // check all franes in frame_index between start and end inclusive
  // if we find a virtual frame, we stop checking and return the frame number
  // if all are non - virtual we return 0
  lives_clip_t *sfile = mainw->files[fileno];

  if (!sfile->frame_index) return  0;
  for (frames_t i = start; i <= end; i++) {
    if (sfile->frame_index[i - 1] != -1) return i;
  }

  return 0;
}


boolean check_if_non_virtual(int fileno, frames_t start, frames_t end) {
  // check if there are no virtual frames from start to end inclusive in clip fileno

  // return FALSE if any virtual frames are found in the region
  // return TRUE if all frames in region are non-virtual

  // also may change the clip_type and the interlace

  lives_clip_t *sfile = mainw->files[fileno];
  frames_t i;

  if (sfile->clip_type != CLIP_TYPE_FILE) return TRUE;

  if (sfile->frame_index) {
    for (i = start; i <= end; i++) {
      if (sfile->frame_index[i - 1] != -1) return FALSE;
    }
  }

  if (start > 1 || end < sfile->frames) return TRUE;

  // no virtual frames in entire clip - change to CLIP_TYPE_DISK

  sfile->clip_type = CLIP_TYPE_DISK;
  del_frame_index(sfile);
  close_clip_decoder(fileno);

  if (sfile->interlace != LIVES_INTERLACE_NONE) {
    sfile->interlace = LIVES_INTERLACE_NONE; // all frames should have been deinterlaced
    sfile->deinterlace = FALSE;
    if (fileno > 0) {
      if (!save_clip_value(fileno, CLIP_DETAILS_INTERLACE, &sfile->interlace))
        do_header_write_error(fileno);
    }
  }

  return TRUE;
}

#define DS_SPACE_CHECK_FRAMES 100

static boolean save_decoded(int fileno, frames_t i, LiVESPixbuf * pixbuf, boolean silent, int progress) {
  lives_clip_t *sfile = mainw->files[fileno];
  boolean retb;
  int retval;
  LiVESError *error = NULL;
  char *oname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));

  do {
    retval = LIVES_RESPONSE_NONE;
    retb = lives_pixbuf_save(pixbuf, oname, sfile->img_type, 100 - prefs->ocp, sfile->hsize, sfile->vsize, &error);
    if (error && !silent) {
      retval = do_write_failed_error_s_with_retry(oname, error->message);
      lives_error_free(error);
      error = NULL;
    } else if (!retb) {
      retval = do_write_failed_error_s_with_retry(oname, NULL);
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


#define STRG_CHECK 100

frames_t virtual_to_images(int sfileno, frames_t sframe, frames_t eframe, boolean update_progress, LiVESPixbuf **pbr) {
  // pull frames from a clip to images
  // from sframe to eframe inclusive (first frame is 1)

  // if update_progress, set mainw->msg with number of frames pulled

  // should be threadsafe apart from progress update

  // if pbr is non-null, it will be set to point to the pulled pixbuf

  // return index of last frame decoded, (negaive value on error)

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESPixbuf *pixbuf = NULL;
  lives_proc_thread_t tinfo = THREADVAR(tinfo);
  savethread_priv_t *saveargs = NULL;
  lives_thread_t *saver_thread = NULL;
  int progress = 1, count = 0;
  frames_t retval = sframe;
  frames_t i;

  if (tinfo) lives_proc_thread_set_cancellable(tinfo);

  if (sframe < 1) sframe = 1;

  for (i = sframe; i <= eframe; i++) {
    if (i > sfile->frames) break;

    retval = i;

    if (sfile->pumper) {
      if (mainw->effects_paused || mainw->preview) {
        lives_nanosleep_until_nonzero((!mainw->effects_paused && !mainw->preview)
                                      || lives_proc_thread_cancelled(sfile->pumper));
      }
      if (lives_proc_thread_cancelled(sfile->pumper)) break;
    }

    if (update_progress) {
      threaded_dialog_spin((double)(i - sframe) / (double)(eframe - sframe + 1));
    }

    if (sfile->frame_index[i - 1] >= 0) {
      if (pbr && pixbuf) lives_widget_object_unref(pixbuf);

      pixbuf = pull_lives_pixbuf_at_size(sfileno, i, get_image_ext_for_type(sfile->img_type),
                                         q_gint64((i - 1.) / sfile->fps, sfile->fps), sfile->hsize,
                                         sfile->vsize, LIVES_INTERP_BEST, FALSE);
      if (!pixbuf) {
        retval = -i;
        break;
      }

      if (!saver_thread) {
        saveargs = (savethread_priv_t *)lives_calloc(1, sizeof(savethread_priv_t));
        saveargs->img_type = sfile->img_type;
        saveargs->compression = 100 - prefs->ocp;
        saveargs->width = sfile->hsize;
        saveargs->height = sfile->vsize;
        saver_thread = (lives_thread_t *)lives_calloc(1, sizeof(lives_thread_t));
      } else {
        lives_thread_join(*saver_thread, NULL);
        while (saveargs->error) {
          check_storage_space(-1, TRUE);
          retval = do_write_failed_error_s_with_retry(saveargs->fname, saveargs->error->message);
          lives_error_free(saveargs->error);
          saveargs->error = NULL;
          if (retval != LIVES_RESPONSE_RETRY) {
            if (saveargs->pixbuf) lives_widget_object_unref(saveargs->pixbuf);
            if (pixbuf) lives_widget_object_unref(pixbuf);
            lives_free(saveargs->fname);
            lives_free(saveargs);
            return -(i - 1);
          }
          lives_pixbuf_save(saveargs->pixbuf, saveargs->fname, saveargs->img_type, saveargs->compression,
                            saveargs->width, saveargs->height, &saveargs->error);
        }
        if (saveargs->pixbuf && saveargs->pixbuf != pixbuf) {
          lives_widget_object_unref(saveargs->pixbuf);
          saveargs->pixbuf = NULL;
        }
        lives_free(saveargs->fname);
        saveargs->fname = NULL;
      }

      saveargs->fname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      saveargs->pixbuf = pixbuf;
      lives_thread_create(saver_thread, LIVES_THRDATTR_NONE, lives_pixbuf_save_threaded, saveargs);

      if (++count == STRG_CHECK) {
        if (!check_storage_space(-1, TRUE)) break;
      }

      // another thread may have called check_if_non_virtual - TODO : use a mutex
      if (!sfile->frame_index) break;
      sfile->frame_index[i - 1] = -1;

      if (update_progress) {
        // sig_progress...
        lives_snprintf(mainw->msg, MAINW_MSG_SIZE, "%d", progress++);
        threaded_dialog_spin((double)(i - sframe) / (double)(eframe - sframe + 1));
        lives_widget_context_update();
      }

      if (mainw->cancelled != CANCEL_NONE && !(sfile->pumper && mainw->preview)) {
        break;
      }
    }
  }

  if (pbr) {
    if (retval > 0) *pbr = pixbuf;
    else *pbr = NULL;
  }
  if (!check_if_non_virtual(sfileno, 1, sfile->frames) && !save_frame_index(sfileno)) {
    check_storage_space(-1, FALSE);
    retval = -i;
  }

  if (saver_thread) {
    lives_thread_join(*saver_thread, NULL);
    if (saveargs->pixbuf && saveargs->pixbuf != pixbuf && (!pbr || *pbr != saveargs->pixbuf)) {
      lives_widget_object_unref(saveargs->pixbuf);
    }
    lives_free(saveargs->fname);
    saveargs->fname = NULL;
    lives_free(saveargs);
  }

  if (pixbuf && (!pbr || *pbr != pixbuf)) {
    lives_widget_object_unref(pixbuf);
  }

  return retval;
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
  lives_memcpy(clipboard->frame_index, ocb->frame_index, clipboard->frames * sizeof(frames_t));
  ncb = clipboard;
  ogamma = ocb->gamma_type;
  ocb->gamma_type = ncb->gamma_type = gamma_type; // force conversion
  for (i = 0; i < clipboard->frames; i++) {
    if (clipboard->frame_index[i] == -1) {
      progress++;
      clipboard = ocb;
      pixbuf = pull_lives_pixbuf_at_size(0, i, get_image_ext_for_type(ocb->img_type),
                                         0, 0, 0, LIVES_INTERP_BEST, FALSE);
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


frames_t realize_all_frames(int clipno, const char *msg, boolean enough, frames_t start, frames_t end) {
  // if enough is set, we show Enough button instead of Cancel.
  frames_t ret;
  int current_file = mainw->current_file;
  mainw->cancelled = CANCEL_NONE;
  if (!IS_VALID_CLIP(clipno)) return 0;

  // in future we may have several versions of the clipboard each with a different gamma type
  // (sRGB, bt.701, etc)
  // here we would restore the version of the clipboard which has gamma type matching the target (current) clip
  // this will avoid quality loss due to constantly converting gamma in the clippboard
  // - This needs further testing before we can allow clips with "native" bt.701 gamma, etc.
  if (clipno == CLIPBOARD_FILE && prefs->btgamma && CURRENT_CLIP_HAS_VIDEO && cfile->gamma_type
      != clipboard->gamma_type) {
    restore_gamma_cb(cfile->gamma_type);
  }

  if (end <= 0) end = mainw->files[clipno]->frames + end;
  if (end < start) return 0;

  if (!check_if_non_virtual(clipno, start, end)) {
    mainw->current_file = clipno;
    cfile->progress_start = start;
    cfile->progress_end = count_virtual_frames(cfile->frame_index, start, end);
    if (enough) mainw->cancel_type = CANCEL_SOFT; // force "Enough" button to be shown
    do_threaded_dialog((char *)msg, TRUE);
    lives_widget_show_all(mainw->proc_ptr->processing);
    mainw->cancel_type = CANCEL_KILL;
    ret = virtual_to_images(mainw->current_file, start, end, TRUE, NULL);
    end_threaded_dialog();
    mainw->current_file = current_file;

    if (mainw->cancelled != CANCEL_NONE) {
      mainw->cancelled = CANCEL_USER;
      return ret;
    }
    if (ret <= 0) return ret;
  }
  return end;
}


void insert_images_in_virtual(int sfileno, frames_t where, frames_t frames, frames_t *frame_index, frames_t start) {
  // insert physical (frames) images (or virtual possibly) into sfile at position where [0 = before first frame]
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESResponseType response;

  char *what;
  frames_t nframes = sfile->frames;
  frames_t i, j = start - 1;

  if (!sfile->frame_index) return;

  what = (_("creating the new frame index for the clip"));
  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  do {
    response = LIVES_RESPONSE_OK;
    create_frame_index(sfileno, FALSE, 0, nframes + frames);
    if (!sfile->frame_index) {
      response = do_memory_error_dialog(what, (nframes + frames) * sizeof(frames_t));
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    return;
  }

  lives_memcpy(sfile->frame_index, sfile->frame_index_back, where * sizeof(frames_t));

  for (i = where; i < where + frames; i++) {
    if (frame_index && frame_index[j] != -1) sfile->frame_index[i] = frame_index[j];
    else sfile->frame_index[i] = -1;
    if (++j >= sfile->frames) j = 0;
  }

  lives_memcpy(&sfile->frame_index[where + frames], &sfile->frame_index_back[where], (nframes - where) * sizeof(frames_t));

  sfile->frames += frames;
  save_frame_index(sfileno);
  sfile->frames -= frames;
}


void delete_frames_from_virtual(int sfileno, frames_t start, frames_t end) {
  // delete (frames) images from sfile at position start to end
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESResponseType response;

  char *what = (_("creating the new frame index for the clip"));
  frames_t nframes = sfile->frames, frames = end - start + 1;

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
    if (!sfile->frame_index) {
      response = do_memory_error_dialog(what, (nframes - frames) * sizeof(frames_t));
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    return;
  }

  lives_memcpy(sfile->frame_index, sfile->frame_index_back, (start - 1) * sizeof(frames_t));
  lives_memcpy(&sfile->frame_index[start - 1], &sfile->frame_index_back[end], (nframes - end) * sizeof(frames_t));

  sfile->frames = nframes - frames;
  save_frame_index(sfileno);
  sfile->frames = nframes;
}


void reverse_frame_index(int sfileno) {
  // reverse order of (virtual) frames in clip (only used for clipboard)
  lives_clip_t *sfile = mainw->files[sfileno];
  int bck;

  if (!sfile || !sfile->frame_index) return;

  for (frames_t i = 0; i < sfile->frames >> 1; i++) {
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

  if (sfile->frame_index) {
    sfile->clip_type = CLIP_TYPE_FILE;
    save_frame_index(sfileno);
  } else {
    del_frame_index(sfile);
    sfile->clip_type = CLIP_TYPE_DISK;
  }
}


void clean_images_from_virtual(lives_clip_t *sfile, frames_t oldsframe, frames_t oldframes) {
  // remove images on disk where the frame_index points to a frame in
  // the original clip

  // only needed if frames were reordered when rendered and the process is
  // then undone

  // oldsframe is > 1 if we rendered to a selection

  // should be threadsafe, provided the frame_index does not change

  // the only purpose of this is to reclaim disk space

  char *iname = NULL;

  if (!sfile || !sfile->frame_index) return;

  for (frames_t i = oldsframe; i <= oldframes; i++) {
    threaded_dialog_spin(0.);
    if ((i <= sfile->frames && sfile->frame_index[i - 1] != -1) || i > sfile->frames) {
      iname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      lives_rm(iname);
    }
  }
}


frames_t *frame_index_copy(frames_t *findex, frames_t nframes, frames_t offset) {
  // copy first nframes from findex and return them, adding offset to each value
  // no checking is done to make sure nframes is in range

  // start at frame offset
  frames_t *findexc = (frames_t *)lives_calloc(nframes, sizeof(frames_t));;
  for (int i = 0; i < nframes; i++) findexc[i] = findex[i + offset];
  return findexc;
}


boolean is_virtual_frame(int sfileno, frames_t frame) {
  // frame is virtual if it is still inside a video clip (read only)
  // once a frame is on disk as an image it is no longer virtual

  // frame starts at 1 here

  // a CLIP_TYPE_FILE with no virtual frames becomes a CLIP_TYPE_DISK

  lives_clip_t *sfile = mainw->files[sfileno];
  if (!IS_VALID_CLIP(sfileno)) return FALSE;
  if (!sfile->frame_index) return FALSE;
  if (frame < 1 || frame > sfile->frames) return FALSE;
  if (sfile->frame_index[frame - 1] != -1) return TRUE;
  return FALSE;
}


void insert_blank_frames(int sfileno, frames_t nframes, frames_t after, int palette) {
  // insert blank frames in clip (only valid just after clip is opened)

  // this is ugly, it should be moved to another file

  lives_clip_t *sfile = mainw->files[sfileno];
  LiVESPixbuf *blankp = NULL;
  LiVESError *error = NULL;
  char oname[PATH_MAX];
  char nname[PATH_MAX];
  char *tmp;

  frames_t i;

  if (first_virtual_frame(sfileno, 1, sfile->frames) != 0) {
    for (i = after + 1; i <= sfile->frames; i++) {
      if (!sfile->frame_index || sfile->frame_index[i - 1] == -1) {
        tmp = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
        lives_snprintf(oname, PATH_MAX, "%s", tmp);
        lives_free(tmp);
        if (lives_file_test(oname, LIVES_FILE_TEST_EXISTS)) {
          tmp = make_image_file_name(sfile, i + nframes, get_image_ext_for_type(sfile->img_type));
          lives_snprintf(nname, PATH_MAX, "%s", tmp);
          lives_free(tmp);
          lives_mv(oname, nname);
          if (THREADVAR(com_failed)) {
            return;
	    // *INDENT-OFF*
          }}}}}
  // *INDENT-ON*

  for (i = after; i < after + nframes; i++) {
    tmp = make_image_file_name(sfile, i + 1, get_image_ext_for_type(sfile->img_type));
    lives_snprintf(oname, PATH_MAX, "%s", tmp);
    lives_free(tmp);
    if (!blankp) blankp = lives_pixbuf_new_blank(sfile->hsize, sfile->vsize, palette);
    lives_pixbuf_save(blankp, oname, sfile->img_type, 100 - prefs->ocp, sfile->hsize, sfile->vsize, &error);
    if (error) {
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

  if (blankp) lives_widget_object_unref(blankp);
}
