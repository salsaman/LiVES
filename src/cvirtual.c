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
#include "callbacks.h"
#include "cvirtual.h"

/** count virtual frames between start and end (inclusive) */
frames_t count_virtual_frames(frames_t *findex, frames_t start, frames_t end) {
  frames_t count = 0;
  for (int i = start - 1; i < end; i++) if (findex[i] != -1) count++;
  return count;
}


frames_t *trim_frame_index(int clipno, frames_t *ref_frame, frames_t start, frames_t len) {
  // make a reordering of frame_index, but omit any sequential frames with identical md5sums
  // each value in alt_frame_index is an index to an entry in frame_index
  // - if sfile has no frame_index, then all frames are images and we invent a vritual frame index filled with the value "-1"
  //
  // this function should only be called at the start of playback for a clip - ie. when startign playback, swithcing clips,
  // looping (but not in ping pong mode) and the end is reached, or when retriggered, or when jumping to a bookmark
  // the value ini_frame can be the index of a normal (un reordered frame), the value returned in it will be the new position
  // in alt_frame_index (ie. frames removed before that index will be subtrcted, or frames after if playing in reverse)
  // if the old index concides with an excluded frame, the value returned will be unchanged, but in the alt_index this will point now
  // to the frame after (or before) the excluded region.
  //
  // start and len can define a region (in un reorded frames) to be scanned, since we cannot determine where this falls in the
  // alt index. A start value of 0 will simply remov any exisitin alt_frame_index;
  /// a positive start is offset from beggining, start < 0 offset from end
  // abs(start > sfile->frames) will simpoly do nothing and return
  // len will always be clamped so start + len - 1 <= sfile->frames or sfile->frames - (start + len - 1) >= 0
  // If start < 0 then this is offset from sfile->frames, and ref_frame is set by reverse tracking
  // Any existing alt_index will be replaced, except in the case where abs(start) > sfile->frames

  //
  // in VJ mode, we do not play audio, so this can be done duting playback, ideally we would parse the start of the clip to skip past
  // blanks and fixed (titles) text

  lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clipno);
  if (sfile) {
    int dir = 1;
    int cut_st = -1;
    int i, j, k = 0, end;
    if (!start) return sfile->alt_frame_index;

    if (start < 0) {
      dir = -1;
      start = -start;
    }
    pthread_mutex_lock(&sfile->frame_index_mutex);
    if (sfile->alt_frame_index) {
      lives_free(sfile->alt_frame_index);
      sfile->alt_frame_index = NULL;
    }
    sfile->alt_frames = 0;

    start--;

    if (start >= sfile->frames) {
      pthread_mutex_unlock(&sfile->frame_index_mutex);
      return sfile->alt_frame_index;
    }

    if (len <= 0 || start + len > sfile->frames) len = sfile->frames - start;

    sfile->alt_frame_index = lives_calloc(sfile->frames, sizeof(frames_t));

    if (dir == 1) end = start + len;
    else {
      end = sfile->frames - 1 - start;
      start -= len;
    }
    for (i = start; i < end; i++) {
      // TODO allow setting of a callback function to determine which entries to filter out
      if ((i + 1 < end && get_frame_md5(clipno, i) && get_frame_md5(clipno, i + 1)
           && !lives_memcmp(get_frame_md5(clipno, i),
                            get_frame_md5(clipno, i + 1), MD5_SIZE))
          || (i + 1 == end && get_frame_md5(clipno, i) && get_frame_md5(clipno, i - 1)
              && !lives_memcmp(get_frame_md5(clipno, i),
                               get_frame_md5(clipno, i - 1), MD5_SIZE))) {
        if (cut_st == -1) cut_st = i;
        continue;
      }
      if (cut_st != -1) {
        cut_st = -1;
        // check audio from cut_st to here
        // if it is silent, or we are in vj mode then we omit the frames from alt_frame_index
        // and we cut out the audio section
        // otherwise we will add all frames from cut_st up to here
        if (1) {
          /* if (!sfile->achans || is_silent((double)cut_st / sfile->fps, (double)i / sfile->fps, sfile->arate, */
          /* 				sfile->achans, sfile->asampsize, sfile->signed_endian)) { */

          /*   if (sfile->achans) cut_audio((double)cut_st / sfile->fps, (double)i / sfile->fps, sfile->srate, */
          /* 			       sfile->achans, sfile->asampsize, sfile->signed_endian); */
          continue;
        }
      } else cut_st = i;
      for (j = cut_st; j <= i; j++) {
        // next frame differs and we are not cutting so we just transfer this
        sfile->alt_frame_index[k] = j;
        sfile->alt_frames++;
        if (ref_frame && j > *ref_frame) {
          if (dir == 1) *ref_frame = k + 1;
          else *ref_frame = k ? k : 1;
          ref_frame = NULL;
        }
        k++;
      }
      cut_st = -1;
    }
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return sfile->alt_frame_index;
  }
  return NULL;
}


boolean create_frame_index(int clipno, boolean init, frames_t start_offset, frames_t nframes) {
  lives_clip_t *sfile;
  size_t idxsize = nframes * sizeof(frames_t);

  if (!IS_PHYSICAL_CLIP(clipno)) return FALSE;

  sfile = mainw->files[clipno];
  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return FALSE;
  }

  // TODO fill with const value so we can better error check
  sfile->frame_index = (frames_t *)lives_calloc_align(idxsize);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return FALSE;
  }
  if (init) for (int i = 0; i < nframes; i++) sfile->frame_index[i] = i + start_offset;
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  return TRUE;
}


// start is old flen, end is nframes in new
static frames_t extend_frame_index(int clipno, frames_t start, frames_t end, frames_t nvframes,
                                   frames_t offs, lives_img_type_t img_type) {
  lives_clip_t *sfile;
  frames_t i;

  if (!IS_PHYSICAL_CLIP(clipno) || start > end) return 0;
  sfile = mainw->files[clipno];
  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (sfile->frame_index_back) lives_free(sfile->frame_index_back);
  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;
  create_frame_index(clipno, TRUE, offs, end);
  if (!sfile->frame_index) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return 0;
  }
  if (start > 0) lives_memcpy(sfile->frame_index, sfile->frame_index_back, start * sizeof(frames_t));
  for (i = start; i < end; i++) {
    char *fname = make_image_file_name(sfile, i + 1, get_image_ext_for_type(img_type));
    if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
      lives_free(fname);
      sfile->frame_index[i] = -1;
    } else {
      lives_free(fname);
      if (nvframes && i >= nvframes) {
        delete_frames_from_virtual(clipno, i, end - 1);
        i = - i;
        break;
      }
    }
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  return i;
}


boolean repair_frame_index(int clipno, frames_t offs) {
  lives_clip_t *sfile = RETURN_PHYSICAL_CLIP(clipno);
  lives_clip_data_t *cdata = get_clip_cdata(clipno);
  if (extend_frame_index(clipno, sfile->old_frames, sfile->frames,
                         cdata ? cdata->nframes : 0, offs, sfile->img_type) == sfile->frames) {
    save_frame_index(clipno);
    return TRUE;
  }
  return FALSE;
}


void repair_findex_cb(LiVESMenuItem *menuitem, livespointer offsp) {
  frames_t oldf = cfile->old_frames;
  if (menuitem) cfile->old_frames = 0;
  // force full reload of decoder so we can check again

  ///< retain original order to restore for freshly opened clips
  // get_decoder_cdata() may alter this
  if (1) {
    LiVESList *odeclist = lives_list_copy(capable->plugins_list[PLUGIN_TYPE_DECODER]);
    int clipno = mainw->current_file;
    char *clipdir = get_clip_dir(clipno);
    char *cwd = lives_get_current_dir();

    remove_primary_src(clipno, LIVES_SRC_TYPE_DECODER);

    lives_chdir(clipdir, FALSE);
    lives_free(clipdir);

    get_decoder_cdata(clipno, NULL);

    lives_chdir(cwd, FALSE);
    lives_free(cwd);

    lives_list_free(capable->plugins_list[PLUGIN_TYPE_DECODER]);
    capable->plugins_list[PLUGIN_TYPE_DECODER] = odeclist;
  }
  if (repair_frame_index(mainw->current_file, LIVES_POINTER_TO_INT(offsp))) {
    switch_clip(1, mainw->current_file, TRUE);
  } else {
    // TODO - show error
  }
  cfile->old_frames = oldf;
}


// save frame_index to disk
boolean save_frame_index(int clipno) {
  int fd, i;
  int retval;
  char *fname, *fname_new, *clipdir;
  lives_clip_t *sfile;

  if (!IS_PHYSICAL_CLIP(clipno)) return FALSE;
  sfile = mainw->files[clipno];

  if (!mainw->is_exiting) pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    if (!mainw->is_exiting) pthread_mutex_unlock(&sfile->frame_index_mutex);
    return FALSE;
  }

  clipdir = get_clip_dir(clipno);
  fname = lives_build_filename(clipdir, FRAME_INDEX_FNAME "." LIVES_FILE_EXT_BACK, NULL);
  fname_new = lives_build_filename(clipdir, FRAME_INDEX_FNAME, NULL);
  lives_free(clipdir);

  do {
    retval = 0;
    fd = lives_create_buffered(fname, DEF_FILE_PERMS);
    if (fd < 0) {
      if (mainw->is_exiting) return FALSE;
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

  pthread_mutex_unlock(&sfile->frame_index_mutex);

  lives_free(fname);
  lives_free(fname_new);

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;

  return TRUE;
}


// load frame_index from disk
// returns -1 (error)
// or maxframe pointed to in clip

frames_t load_frame_index(int clipno) {
  lives_clip_t *sfile;
  off_t filesize;
  char *fname, *fname_back;
  char *clipdir;
  boolean backuptried = FALSE;
  frames_t maxframe = -1;
  int fd, retval, i;

  if (!IS_PHYSICAL_CLIP(clipno)) return -1;

  sfile = mainw->files[clipno];
  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return -1;
  }

  clipdir = get_clip_dir(clipno);
  fname = lives_build_filename(clipdir, FRAME_INDEX_FNAME, NULL);
  filesize = sget_file_size(fname);

  if (filesize <= 0) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    if (clipdir) lives_free(clipdir);
    lives_free(fname);
    return 0;
  }

  if (filesize >> 2 > (off_t)sfile->frames) sfile->frames = (frames_t)(filesize >> 2);
  fname_back = lives_build_filename(clipdir, FRAME_INDEX_FNAME "." LIVES_FILE_EXT_BACK, NULL);
  if (clipdir) lives_free(clipdir);

  do {
    retval = 0;
    fd = lives_open_buffered_rdonly(fname);
    if (fd < 0) {
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
          sfile->frame_index_back = NULL;
        }
      }
      if (retval == LIVES_RESPONSE_CANCEL) {
        pthread_mutex_unlock(&sfile->frame_index_mutex);
        lives_free(fname);
        lives_free(fname_back);
        return -1;
      }
    } else {
      LiVESResponseType response;
      char *what = (_("creating the frame index for the clip"));
      do {
        response = LIVES_RESPONSE_OK;
        create_frame_index(clipno, FALSE, 0, sfile->frames);
        if (!cfile->frame_index) {
          response = do_memory_error_dialog(what, sfile->frames * sizeof(frames_t));
        }
      } while (response == LIVES_RESPONSE_RETRY);
      lives_free(what);
      if (response == LIVES_RESPONSE_CANCEL) {
        break;
      }

      for (i = 0; i < sfile->frames; i++) {
        lives_read_le_buffered(fd, &sfile->frame_index[i], sizeof(frames_t), TRUE);
        if (THREADVAR(read_failed)) {
          g_print("only read %d of %d frames from index\n", i, sfile->frames);
          break;
        }
        if (sfile->frame_index[i] > maxframe) {
          maxframe = sfile->frame_index[i];
        }
      }
      lives_close_buffered(fd);

      if (THREADVAR(read_failed)) {
        THREADVAR(thrdnative_flags) |= THRDNATIVE_CAN_CORRECT;
        retval = do_read_failed_error_s_with_retry(fname, NULL);
        THREADVAR(thrdnative_flags) &= ~THRDNATIVE_CAN_CORRECT;
        if (retval == LIVES_RESPONSE_CANCEL) {
          sfile->old_frames = i;
          repair_findex_cb(NULL, LIVES_INT_TO_POINTER(0));
        }
      }

      if (!backuptried) {
        backuptried = TRUE;
        fd = lives_open_buffered_rdonly(fname_back);
        if (fd >= 0) {
          LiVESList *list = NULL;
          frames_t vframe;
          int count = 0;
          for (; lives_read_le_buffered(fd, &vframe,
                                        sizeof(frames_t), TRUE) == sizeof(frames_t); count++) {
            if (THREADVAR(read_failed)) break;
            list = lives_list_prepend(list, LIVES_INT_TO_POINTER(vframe));
          }
          lives_close_buffered(fd);
          if (THREADVAR(read_failed)) {
            THREADVAR(read_failed) = 0;
          } else if (count) {
            frames_t *f_index = sfile->frame_index;
            list = lives_list_reverse(list);
            lives_freep((void **)&sfile->frame_index_back);
            sfile->frame_index = NULL;
            create_frame_index(clipno, FALSE, 0, count);
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

  pthread_mutex_unlock(&sfile->frame_index_mutex);
  lives_free(fname);
  lives_free(fname_back);

  if (maxframe >= 0) sfile->clip_type = CLIP_TYPE_FILE;
  return ++maxframe;
}


void del_frame_index(int clipno) {
  // physically delete the frame_index for a clip
  // only done once all
  lives_clip_t *sfile;
  char *idxfile;

  if (!IS_PHYSICAL_CLIP(clipno)) return;
  sfile = mainw->files[clipno];

  // cannot call check_if_non_virtual() else we end up recursing

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (sfile->frame_index) {
    for (frames_t i = 1; i <= sfile->frames; i++) {
      if (sfile->frame_index[i - 1] != -1) {
        pthread_mutex_unlock(&sfile->frame_index_mutex);
        LIVES_ERROR("deleting frame_index with virtual frames in it !");
        return;
      }
    }
  }

  if (sfile != clipboard) {
    char *clipdir = get_clip_dir(clipno);
    idxfile = lives_build_filename(clipdir, FRAME_INDEX_FNAME, NULL);
    lives_rm(idxfile);
    lives_free(idxfile);
    lives_free(clipdir);
  }

  lives_freep((void **)&sfile->frame_index);
  pthread_mutex_unlock(&sfile->frame_index_mutex);
}


static frames_t scan_frames(lives_clip_t *sfile, frames_t vframes, frames_t last_real_frame) {
  frames_t i;
  if (!sfile) return 0;

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return last_real_frame;
  }
  for (i = 0; i < sfile->frames; i++) {
    // assume all real frames up to last_real_frame are there
    if ((sfile->frame_index[i] == -1 && i >= last_real_frame) || (sfile->frame_index[i] > vframes)) {
      pthread_mutex_unlock(&sfile->frame_index_mutex);
      return i;
    }
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  return i;
}


lives_img_type_t resolve_img_type(lives_clip_t *sfile) {
  lives_img_type_t ximgtype;
  int nimty = (int)N_IMG_TYPES;
  char *fname;
  for (frames_t i = sfile->frames; i--;) {
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


boolean check_clip_integrity(int clipno, const lives_clip_data_t *cdata, frames_t maxframe) {
  // TODO - if we have binfmt, check md5sum, afilesize, video_time, laudio_time, etc.
  // maxframe is highest vframe reffed in frame_index
  // sfile->frames should be len of frame_index
  // last_real_frame will point to highest 'real' frame detected
  lives_clip_t *sfile = mainw->files[clipno], *binf = NULL;
  lives_img_type_t empirical_img_type = sfile->img_type, oemp = empirical_img_type;
  lives_img_type_t ximgtype;
  frames_t last_real_frame = sfile->frames, last_img_frame;
  int nimty = (int)N_IMG_TYPES, j;
  boolean has_missing_frames = FALSE, bad_imgfmts = FALSE, do_rescan = FALSE;
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

  // TODO: write errors to textbuffer type log

  if (prefs->vj_mode) return TRUE;
  sfile->old_frames = sfile->frames;

  if (sfile->frames) {
    sfile->afilesize = reget_afilesize_inner(clipno);
    get_total_time(sfile);
    if (sfile->laudio_time - sfile->video_time > AV_TRACK_MIN_DIFF) {
      if (prefs->show_dev_opts) {
        g_printerr("AV timing mismatch: video time == %f sec, audio time == %f\n",
                   sfile->video_time, sfile->laudio_time);
      }
      binf = clip_forensic(clipno, NULL);
      if (binf && binf->frames) {
        if (binf->frames * sfile->fps == sfile->laudio_time
            || binf->frames * binf->fps == sfile->laudio_time
            || (cdata && binf->frames * cdata->fps == sfile->laudio_time)) {
          has_missing_frames = TRUE;
        }
      } else {
        if (prefs->show_dev_opts) {
          g_printerr("no binfmt->frames\n");
        }
        do_rescan = TRUE;
      }
    }
  }
  // check the image type

  for (i = sfile->frames; i--;) {
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
              if (oemp == IMG_TYPE_UNKNOWN) {
                if (prefs->show_dev_opts) {
                  g_printerr("file integrity: guessing img type is %s\n",
                             image_ext_to_lives_image_type(get_image_ext_for_type(ximgtype)));
                }
                oemp = ximgtype;
              }
            } else {
              if (ximgtype == oemp) empirical_img_type = oemp;
              if (prefs->show_dev_opts) {
                g_printerr("clip %s has wrong img formats\n", sfile->handle);
              }
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
    // maxframe is highest frame in src clip, cdata->nframes should be nframes in src clip

    if (maxframe > cdata->nframes || has_missing_frames || do_rescan) {
      if (prefs->show_dev_opts) {
        if (maxframe > cdata->nframes) {
          g_printerr("frame count mismatch for clip %d,  %s, maxframe is %d, decoder claims only %ld\nRescaning...",
                     clipno, sfile->handle, maxframe, cdata-> nframes);
        }
      }
      sfile->frames = scan_frames(sfile, cdata->nframes, last_real_frame);
      if (prefs->show_dev_opts) {
        g_printerr("rescan counted %d frames (expected %d)\n.", sfile->frames, maxframe);
      }
      if (last_real_frame > sfile->frames) has_missing_frames = TRUE;
    }
  }

  if (sfile->frame_index) {
    frames_t lgoodframe = -1;
    int goodidx;
    frames_t xframes = sfile->frames;

    if (sfile->frame_index_back) {
      // old_frames should have been set in load_frame_index()
      if (sfile->old_frames >= sfile->frames) {
        xframes = sfile->old_frames;
      }
      // start by assuming backup is more correct
      backup_more_correct = TRUE;
      if (xframes > sfile->frames) {
        sfile->old_frames = sfile->frames = abs(extend_frame_index(clipno, sfile->frames, xframes,
                                                cdata ? cdata->nframes : 0, 0,
                                                empirical_img_type));
      }
    }
    // check and attempt to correct frame_index
    for (i = 0; i < xframes; i++) {
      frames_t fr;

      // check frame_index first, unless we have passed its end
      if (i < sfile->frames || !sfile->frame_index_back) fr = sfile->frame_index[i];
      else fr = sfile->frame_index_back[i];

      if (fr < -1 || (!cdata && (frames64_t)fr > sfile->frames - 1)
          || (cdata && (frames64_t)fr > cdata->nframes - 1)) {
        if (i >= sfile->frames && sfile->frame_index_back) {
          // past the end so it must have been from backup
          backup_more_correct = FALSE;
          break;
        }
        if (backup_more_correct && i < sfile->old_frames) {
          frames_t fr2 = sfile->frame_index_back[i];
          if (fr2 < -1 || (!cdata && (frames64_t)fr2 > sfile->frames - 1)
              || (cdata && (frames64_t)fr2 > cdata->nframes - 1)) {
            // backup was incorrect, remove the assumption
            backup_more_correct = FALSE;
            xframes = sfile->frames;
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
          // image file missing
          if (backup_more_correct && i < sfile->old_frames && sfile->frame_index_back[i] == -1) {
            backup_more_correct = FALSE;
            xframes = sfile->frames;
          }
          // relink it to next virtual frame if we can
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
          if (cdata) {
            if (sfile->frame_index[i] >= cdata->nframes) sfile->frame_index[i] = cdata->nframes - 1;
          } else {
            //
          }
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
    sfile->frame_index_back = NULL;
  }

  if (cdata && !sfile->frame_index) {
    sfile->frames = sfile->old_frames = MAX(cdata->nframes, last_real_frame);
    create_frame_index(clipno, TRUE, 0, sfile->frames);
    if (last_real_frame > cdata->nframes) {
      for (i = last_real_frame - 1; i > cdata->nframes; i--) {
        fname = make_image_file_name(sfile, i, get_image_ext_for_type(empirical_img_type));
        if (!lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
          last_real_frame = sfile->frames = i - 1;
        } else sfile->frame_index[i - 1] = -1;
      }
      if (last_real_frame == cdata->nframes) {
        fname = make_image_file_name(sfile, last_real_frame, get_image_ext_for_type(empirical_img_type));
        if (!lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
          last_real_frame = 0;
        }
      }
      last_img_frame = cdata->nframes;
    } else last_img_frame = last_real_frame - 1;
    if (sfile->frames < sfile->old_frames) {
      delete_frames_from_virtual(clipno, sfile->frames + 1, sfile->old_frames);
      sfile->old_frames = sfile->frames;
    }
    for (i = 1; i <= last_img_frame; i++) {
      fname = make_image_file_name(sfile, i, get_image_ext_for_type(empirical_img_type));
      if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
        sfile->frame_index[i - 1] = -1;
        if (i > last_real_frame) last_real_frame = i;
      }
    }
    backup_more_correct = FALSE;
    has_missing_frames = FALSE;
    save_frame_index(clipno);
  }

  if (has_missing_frames && backup_more_correct) {
    if (sfile->old_frames > sfile->frames) {
      sfile->frames = sfile->old_frames;
      sfile->frames = scan_frames(sfile, sfile->frames, last_real_frame);
    } else sfile->frames = sfile->old_frames;
  }

  if (sfile->frames > 0) {
    int hsize = sfile->hsize, chsize = hsize;
    int vsize = sfile->vsize, cvsize = vsize;
    last_img_frame = -1;
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
        if (sfile->img_type != empirical_img_type) {
          sfile->img_type = empirical_img_type;
          save_clip_value(clipno, CLIP_DETAILS_IMG_TYPE, &sfile->img_type);
        }
        get_frames_sizes(clipno, last_img_frame, &hsize, &vsize);
      }
    }

    if (cdata) {
      chsize = cdata->width * weed_palette_get_pixels_per_macropixel(cdata->current_palette) / VSLICES;
      cvsize = cdata->height;
    }

    hsize /= VSLICES;

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
            if (!sfile->frame_index || sfile->frame_index[i - 1] == -1) {
              fname = make_image_file_name(sfile, i, get_image_ext_for_type(empirical_img_type));
              if (lives_file_test(fname, LIVES_FILE_TEST_EXISTS)) {
                fframe = i;
                lives_free(fname);
                break;
              }
              if (prefs->show_dev_opts) {
                g_printerr("img frame not found\n");
              }
              has_missing_frames = TRUE;
              lives_free(fname);
	      // *INDENT-OFF*
	    }}}
	if (fframe) get_frames_sizes(clipno, fframe, &hsize, &vsize);
      }}
    // *INDENT-ON*

    if (sfile->hsize == 0 && sfile->vsize == 0 && hsize > 0 && vsize > 0) {
      sfile->hsize = hsize;
      sfile->vsize = vsize;
    }

    hsize /= VSLICES;

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
        if (resize_all(clipno, sfile->hsize, sfile->vsize, empirical_img_type, FALSE,
                       &nbadsized, &missing)) {
          g_printerr("resize detected %d bad sized, %d missing \n", nbadsized, missing);
          if (missing) has_missing_frames = TRUE;
          if (mainw->cancelled == CANCEL_NONE) bad_imgfmts = FALSE;
          else mismatch = TRUE;
        }
        mainw->cancelled = CANCEL_NONE;
        end_threaded_dialog();
        threaded_dialog_pop();
      } else {
        if (prefs->show_dev_opts) {
          g_printerr("bad frame sizes detected\n");
        }
        mismatch = TRUE;
      }
    }
  }
  if (bad_imgfmts) {
    LiVESResponseType resp = do_imgfmts_error(empirical_img_type);
    if (resp == LIVES_RESPONSE_OK) {
      int missing = 0, nbadsized = 0;
      threaded_dialog_push();
      do_threaded_dialog(_("Correcting Image Formats\n"), TRUE);
      if (resize_all(clipno, sfile->hsize, sfile->vsize, empirical_img_type, FALSE,
                     &nbadsized, &missing)) {
        g_printerr("change fmts detected %d bad sized, %d missing \n", nbadsized, missing);
        if (missing) has_missing_frames = TRUE;
        if (mainw->cancelled == CANCEL_NONE) bad_imgfmts = FALSE;
        else mismatch = TRUE;
      }
      mainw->cancelled = CANCEL_NONE;
      end_threaded_dialog();
      threaded_dialog_pop();
    } else {
      if (prefs->show_dev_opts) {
        g_printerr("Img format error\n");
      }
      mismatch = TRUE;
    }
    mainw->cancelled = CANCEL_NONE;
  }

  if (has_missing_frames) {
    if (prefs->show_dev_opts) {
      g_printerr("missing frames detected\n");
    }
    mismatch = TRUE;
  } else {
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
  if (cdata && (fdim(sfile->fps, (double)cdata->fps) > prefs->fps_tolerance)) {
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
  if (!binf) binf = clip_forensic(clipno, NULL);

  if (binf) {
    if (cdata && binf->fps == cdata->fps)  {
      sfile->pb_fps = sfile->fps = cdata->fps;
    }
    if (has_missing_frames) {
      if (cdata) {
        if (binf->frames == cdata->nframes && binf->frames < sfile->frames) sfile->frames = binf->frames;
        else if ((binf->frames == sfile->frames && binf->frames != cdata->nframes)
                 || (sfile->video_time < sfile->laudio_time
                     && binf->frames * sfile->fps == sfile->laudio_time)) {
          sfile->frames = binf->frames;
          if (sfile->frames > cdata->nframes) {
            sfile->frames = sfile->old_frames = abs(extend_frame_index(clipno, cdata->nframes,
                                                    sfile->frames, cdata->nframes,
                                                    0, empirical_img_type));
            save_frame_index(clipno);
          }
          maxframe = sfile->frames;
        }
      } else if (binf->frames <= sfile->frames) sfile->frames = binf->frames;
    }
  }

  sfile->img_type = empirical_img_type;

  sfile->needs_update = TRUE;

  sfile->afilesize = reget_afilesize_inner(clipno);

  if (has_missing_frames && sfile->frame_index) {
    if (sfile->frames > sfile->old_frames)
      sfile->frames = sfile->old_frames = abs(extend_frame_index(clipno, maxframe, sfile->frames,
                                              cdata ? cdata->nframes : 0, 0,
                                              empirical_img_type));
    save_frame_index(clipno);
  }
  return FALSE;
}


/* frames_t first_virtual_frame(int clipno, frames_t start, frames_t end) { */
/*   // check all franes in frame_index between start and end inclusive */
/*   // if we find a virtual frame, we stop checking and return the frame number */
/*   // if all are non - virtual we return 0 */
/*   lives_clip_t *sfile; */
/*   if (!IS_PHYSICAL_CLIP(clipno)) return 0; */

/*   sfile = mainw->files[clipno]; */

/*   pthread_mutex_lock(&sfile->frame_index_mutex); */
/*   if (!sfile->frame_index) { */
/*     pthread_mutex_unlock(&sfile->frame_index_mutex); */
/*     return  0; */
/*   } */
/*   for (frames_t i = start; i <= end; i++) { */
/*     if (sfile->frame_index[i - 1] != -1) { */
/*       pthread_mutex_unlock(&sfile->frame_index_mutex); */
/*       return i; */
/*     } */
/*   } */
/*   pthread_mutex_unlock(&sfile->frame_index_mutex); */
/*   return 0; */
/* } */


boolean check_if_non_virtual(int clipno, frames_t start, frames_t end) {
  // check if there are no virtual frames from start to end inclusive in clip clipno

  // return FALSE if any virtual frames are found in the region
  // return TRUE if all frames in region are non-virtual

  // also may change the clip_type and the interlace

  lives_clip_t *sfile;
  frames_t i;

  if (!IS_PHYSICAL_CLIP(clipno)) return TRUE;
  sfile = mainw->files[clipno];

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return TRUE;
  }
  for (i = start; i <= end; i++) {
    if (sfile->frame_index[i - 1] != -1) {
      pthread_mutex_unlock(&sfile->frame_index_mutex);
      return FALSE;
    }
  }

  if (start > 1 || end < sfile->frames) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return TRUE;
  }
  // no virtual frames in entire clip - change to CLIP_TYPE_DISK

  del_frame_index(clipno);
  pthread_mutex_unlock(&sfile->frame_index_mutex);

  sfile->clip_type = CLIP_TYPE_DISK;

  remove_primary_src(clipno, LIVES_SRC_TYPE_DECODER);

  sfile->old_dec_uid = 0;
  if (mainw->is_ready) {
    sfile->old_dec_uid = sfile->decoder_uid;
    del_clip_value(clipno, CLIP_DETAILS_DECODER_NAME);
    del_clip_value(clipno, CLIP_DETAILS_DECODER_UID);
  } else if (!sfile->needs_update) sfile->needs_silent_update = TRUE;

  if (sfile->interlace != LIVES_INTERLACE_NONE) {
    sfile->old_interlace = sfile->interlace;
    sfile->interlace = LIVES_INTERLACE_NONE; // all frames should have been deinterlaced
    sfile->deinterlace = FALSE;
    if (mainw->is_ready) {
      if (clipno > 0) {
        if (!save_clip_value(clipno, CLIP_DETAILS_INTERLACE, &sfile->interlace))
          do_header_write_error(clipno);
      }
    } else if (!sfile->needs_update) sfile->needs_silent_update = TRUE;
  }

  return TRUE;
}

#define DS_SPACE_CHECK_FRAMES 100

static boolean save_decoded(int clipno, frames_t i, LiVESPixbuf * pixbuf, boolean silent, int progress) {
  LiVESError *error = NULL;
  lives_clip_t *sfile;
  char *oname;
  boolean retb;
  int retval;

  if (!IS_PHYSICAL_CLIP(clipno)) return FALSE;
  sfile = mainw->files[clipno];
  oname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
  do {
    retval = LIVES_RESPONSE_NONE;
    retb = pixbuf_to_png(pixbuf, oname, sfile->img_type, 100 - prefs->ocp, sfile->hsize, sfile->vsize, &error);
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
    if (!check_storage_space(clipno, FALSE)) {
      retval = LIVES_RESPONSE_CANCEL;
    }
  }

  if (retval == LIVES_RESPONSE_CANCEL) return FALSE;
  return TRUE;
}


#define STRG_CHECK 100

frames_t virtual_to_images(int sclipno, frames_t sframe, frames_t eframe, boolean update_progress, LiVESPixbuf **pbr) {
  // pull frames from a clip to images
  // from sframe to eframe inclusive (first frame is 1)

  // if update_progress, set mainw->msg with number of frames pulled

  // should be threadsafe apart from progress update

  // if pbr is non-null, it will be set to point to the pulled pixbuf

  // return index of last frame decoded, (negaive value on error)

  // we will use the THUMBNAIL srcgroup to pull the images

  lives_clipsrc_group_t *srcgrp = NULL;
  lives_proc_thread_t saver_procthread;
  lives_clip_t *sfile;
  LiVESPixbuf *pixbuf = NULL;
  weed_layer_t *layer = NULL;
  GET_PROC_THREAD_SELF(self);
  savethread_priv_t *saveargs = NULL;
  lives_thread_t *saver_thread = NULL;
  int progress = 1, count = 0;
  frames_t retval = sframe;
  frames_t i;
  boolean intimg = FALSE;
  short pbq = prefs->pb_quality;

  if (lives_proc_thread_get_cancel_requested(self))
    lives_proc_thread_cancel_self(self);

  sfile = RETURN_PHYSICAL_CLIP(sclipno);
  if (!sfile) return -1;

  if (sframe > eframe) {
    frames_t tmp = sframe;
    eframe = sframe;
    sframe = tmp;
  }

  if (sframe > sfile->frames) return sfile->frames;
  if (eframe < 1) return 0;

  if (eframe > sfile->frames) eframe = sfile->frames;
  if (sframe < 1) sframe = 1;

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return eframe;
  }

  //

  prefs->pb_quality = PB_QUALITY_BEST;

  // use internal image saver if we can
  if (sfile->img_type == IMG_TYPE_PNG) intimg = TRUE;

  saveargs = (savethread_priv_t *)lives_calloc(1, sizeof(savethread_priv_t));
  saveargs->img_type = sfile->img_type;
  saveargs->compression = 100 - prefs->ocp;
  saveargs->width = sfile->hsize;
  saveargs->height = sfile->vsize;

  if (intimg) {
    saver_procthread = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                       layer_to_png_threaded, WEED_SEED_BOOLEAN, "v", saveargs);
  } else {
    saver_procthread = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                       pixbuf_to_png_threaded, WEED_SEED_BOOLEAN, "v", saveargs);
  }

  for (i = sframe; i <= eframe; i++) {
    if (lives_proc_thread_get_cancel_requested(self)) break;

    retval = i;

    if (sfile->pumper) {
      if (mainw->effects_paused || mainw->preview) {
        lives_sleep_while_true((mainw->effects_paused || mainw->preview)
                                   && !lives_proc_thread_get_cancel_requested(sfile->pumper));
      }
      if (lives_proc_thread_get_cancel_requested(self)) break;
    }

    if (update_progress) {
      threaded_dialog_spin((double)(i - sframe) / (double)(eframe - sframe + 1));
    }

    if (sfile->frame_index[i - 1] >= 0) {
      if (pbr && pixbuf) lives_widget_object_unref(pixbuf);
      if (intimg) {
        int palette, tpal;
        layer = lives_layer_new_for_frame(sclipno, i);
        if (!srcgrp) srcgrp = get_srcgrp(sclipno, -1, SRC_PURPOSE_THUMBNAIL);
        if (!srcgrp) srcgrp = clone_srcgrp(sclipno, sclipno, -1, SRC_PURPOSE_THUMBNAIL);
        lives_layer_set_srcgrp(layer, srcgrp);
        if (!pull_frame(layer, NULL, 0)) {
          retval = -i;
          break;
        }
        if (lives_proc_thread_get_cancel_requested(self)) break;
        palette = weed_layer_get_palette(layer);
        if (weed_palette_has_alpha(palette)) {
          tpal = WEED_PALETTE_RGBA32;
        } else {
          tpal = WEED_PALETTE_RGB24;
        }
        if (!convert_layer_palette_full(layer, tpal, 0, 0, 0, WEED_GAMMA_SRGB)) {
          retval = -i;
          break;
        }
        gamma_convert_layer(WEED_GAMMA_SRGB, layer);
      } else {
        pixbuf = pull_lives_pixbuf_at_size(sclipno, i, get_image_ext_for_type(sfile->img_type),
                                           q_gint64((i - 1.) / sfile->fps, sfile->fps), sfile->hsize,
                                           sfile->vsize, LIVES_INTERP_BEST, FALSE);
        if (!pixbuf) {
          retval = -i;
          break;
        }
      }

      if (lives_proc_thread_get_cancel_requested(self)) break;

      if (lives_proc_thread_is_unqueued(saver_procthread)) {
        // queue it
        goto queue_lpt;
      }

      if (!lives_proc_thread_check_finished(saver_procthread)) {
        lives_sleep_while_false(lives_proc_thread_check_finished(saver_procthread));
      }

      if (saveargs->error || THREADVAR(write_failed)) {
        THREADVAR(write_failed) = 0;
        check_storage_space(-1, TRUE);
        retval = do_write_failed_error_s_with_retry(saveargs->fname,
                 saveargs->error ? saveargs->error->message
                 : NULL);
        if (saveargs->error) {
          lives_error_free(saveargs->error);
          saveargs->error = NULL;
        }
        if (retval != LIVES_RESPONSE_RETRY) {
          pthread_mutex_unlock(&sfile->frame_index_mutex);
          if (intimg) {
            if (saveargs->layer) weed_layer_free(saveargs->layer);
          } else {
            if (saveargs->pixbuf) lives_widget_object_unref(saveargs->pixbuf);
            if (pixbuf) lives_widget_object_unref(pixbuf);
          }
          lives_free(saveargs->fname);
          lives_free(saveargs);
          return -(i - 1);
        }
        if (intimg) {
          if (saveargs->layer) weed_layer_free(saveargs->layer);
          saveargs->layer = NULL;
        } else {
          if (saveargs->pixbuf && saveargs->pixbuf != pixbuf) {
            lives_widget_object_unref(saveargs->pixbuf);
            saveargs->pixbuf = NULL;
          }
        }
        lives_free(saveargs->fname);
        saveargs->fname = NULL;
      }

      if (lives_proc_thread_get_cancel_requested(self)) break;

queue_lpt:
      saveargs->fname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      if (intimg) {
        saveargs->layer = layer;
      } else {
        saveargs->pixbuf = pixbuf;
      }
      lives_proc_thread_queue(saver_procthread, 0);

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

      if (lives_proc_thread_get_cancel_requested(self)) break;
      if (mainw->cancelled != CANCEL_NONE && !(sfile->pumper && mainw->preview)) {
        break;
      }
    }
  }

  pthread_mutex_unlock(&sfile->frame_index_mutex);

  if (saver_thread) {
    if (intimg) {
      if (saveargs->layer != layer)
        weed_layer_unref(saveargs->layer);
    } else {
      if (saveargs->pixbuf && saveargs->pixbuf != pixbuf && (!pbr || *pbr != saveargs->pixbuf)) {
        lives_widget_object_unref(saveargs->pixbuf);
      }
    }
    lives_free(saveargs->fname);
    saveargs->fname = NULL;
    lives_free(saveargs);
  }

  if (pbr) {
    if (retval > 0) {
      if (intimg) {
        *pbr = layer_to_pixbuf(layer, TRUE, FALSE);
      } else *pbr = pixbuf;
    } else *pbr = NULL;
  }

  prefs->pb_quality = pbq;

  if (!check_if_non_virtual(sclipno, 1, sfile->frames) && !save_frame_index(sclipno)) {
    check_storage_space(-1, FALSE);
    retval = -i;
  }

  if (!intimg) {
    if (pixbuf && (!pbr || *pbr != pixbuf)) {
      lives_widget_object_unref(pixbuf);
    }
  } else if (layer) weed_layer_unref(layer);

  if (lives_proc_thread_get_cancel_requested(sfile->pumper))
    lives_proc_thread_cancel_self(self);

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
  lives_clip_t *sfile;
  frames_t ret;
  int current_file = mainw->current_file;
  mainw->cancelled = CANCEL_NONE;

  if (!IS_PHYSICAL_CLIP(clipno)) return -1;

  sfile = mainw->files[clipno];

  // in future we may have several versions of the clipboard each with a different gamma type
  // (sRGB, bt.701, etc)
  // here we would restore the version of the clipboard which has gamma type matching the target (current) clip
  // this will avoid quality loss due to constantly converting gamma in the clippboard
  // - This needs further testing before we can allow clips with "native" bt.701 gamma, etc.
  if (clipno == CLIPBOARD_FILE && prefs->btgamma && CLIP_HAS_VIDEO(clipno) && sfile->gamma_type
      != clipboard->gamma_type) {
    restore_gamma_cb(sfile->gamma_type);
  }

  if (end <= 0) end = sfile->frames + end;
  if (end < start) return 0;

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!check_if_non_virtual(clipno, start, end)) {
    mainw->current_file = clipno;
    sfile->progress_start = start;
    sfile->progress_end = count_virtual_frames(sfile->frame_index, start, end);
    if (enough) mainw->cancel_type = CANCEL_SOFT; // force "Enough" button to be shown
    do_threaded_dialog((char *)msg, TRUE);
    lives_widget_show_all(mainw->proc_ptr->processing);
    mainw->cancel_type = CANCEL_KILL;
    ret = virtual_to_images(clipno, start, end, TRUE, NULL);
    end_threaded_dialog();
    mainw->current_file = current_file;

    if (mainw->cancelled != CANCEL_NONE) {
      mainw->cancelled = CANCEL_USER;
      end = ret;
    } else if (ret <= 0) end = ret;
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  return end;
}


void insert_images_in_virtual(int sclipno, frames_t where, frames_t frames, frames_t *frame_index, frames_t start) {
  // insert physical (frames) images (or virtual possibly) into sfile at position where [0 = before first frame]
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile;
  LiVESResponseType response;

  char *what;
  frames_t nframes, i, j = start - 1;

  if (!IS_PHYSICAL_CLIP(sclipno)) return;
  sfile = mainw->files[sclipno];

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return;
  }

  what = (_("creating the new frame index for the clip"));
  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;
  nframes = sfile->frames;

  do {
    response = LIVES_RESPONSE_OK;
    create_frame_index(sclipno, FALSE, 0, nframes + frames);
    if (!sfile->frame_index) {
      response = do_memory_error_dialog(what, (nframes + frames) * sizeof(frames_t));
    }
  } while (response == LIVES_RESPONSE_RETRY);
  lives_free(what);
  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    pthread_mutex_unlock(&sfile->frame_index_mutex);
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
  save_frame_index(sclipno);
  sfile->frames -= frames;
  pthread_mutex_unlock(&sfile->frame_index_mutex);
}


void delete_frames_from_virtual(int sclipno, frames_t start, frames_t end) {
  // delete (frames) images from sfile at position start to end (inclusive)
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile;
  char *what;
  LiVESResponseType response;
  frames_t nframes, frames = end - start + 1;

  if (!IS_PHYSICAL_CLIP(sclipno)) return;
  sfile = mainw->files[sclipno];

  pthread_mutex_lock(&sfile->frame_index_mutex);
  lives_freep((void **)&sfile->frame_index_back);

  sfile->frame_index_back = sfile->frame_index;
  sfile->frame_index = NULL;

  nframes = sfile->frames;

  if (nframes - frames == 0) {
    del_frame_index(sclipno);
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return;
  }

  what = (_("creating the new frame index for the clip"));

  do {
    response = LIVES_RESPONSE_OK;
    create_frame_index(sclipno, FALSE, 0, nframes - frames);
    if (!sfile->frame_index) {
      response = do_memory_error_dialog(what, (nframes - frames) * sizeof(frames_t));
    }
  } while (response == LIVES_RESPONSE_RETRY);

  lives_free(what);

  if (response == LIVES_RESPONSE_CANCEL) {
    sfile->frame_index = sfile->frame_index_back;
    sfile->frame_index_back = NULL;
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return;
  }

  lives_memcpy(sfile->frame_index, sfile->frame_index_back, (start - 1) * sizeof(frames_t));
  lives_memcpy(&sfile->frame_index[start - 1], &sfile->frame_index_back[end], (nframes - end) * sizeof(frames_t));

  sfile->frames = nframes - frames;
  save_frame_index(sclipno);
  sfile->frames = nframes;
  pthread_mutex_unlock(&sfile->frame_index_mutex);
}


void reverse_frame_index(int sclipno) {
  // reverse order of (virtual) frames in clip (only used for clipboard)
  lives_clip_t *sfile;

  if (!IS_PHYSICAL_CLIP(sclipno)) return;

  sfile = mainw->files[sclipno];
  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return;
  }

  for (frames_t i = 0; i < sfile->frames >> 1; i++) {
    int bck = sfile->frame_index[i];
    sfile->frame_index[i] = sfile->frame_index[sfile->frames - 1 - i];
    sfile->frame_index[sfile->frames - 1 - i] = bck;
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
}


void restore_frame_index_back(int sclipno) {
  // undo an operation
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  lives_clip_t *sfile;
  if (!IS_PHYSICAL_CLIP(sclipno)) return;

  sfile = mainw->files[sclipno];

  pthread_mutex_lock(&sfile->frame_index_mutex);
  lives_freep((void **)&sfile->frame_index);
  sfile->frame_index = sfile->frame_index_back;
  sfile->frame_index_back = NULL;

  if (sfile->frame_index) {
    boolean bad_header = FALSE;
    if (sfile->clip_type == CLIP_TYPE_DISK) {
      if (sfile->old_interlace != sfile->interlace) {
        sfile->interlace = sfile->old_interlace;
        if (sfile->interlace != LIVES_INTERLACE_NONE) sfile->deinterlace = TRUE;
        if (!save_clip_value(sclipno, CLIP_DETAILS_INTERLACE, &sfile->interlace))
          bad_header = TRUE;
      }
      if (!bad_header) {
        lives_clip_src_t *dsource = get_primary_src(sclipno);
        if (dsource && sfile->old_dec_uid) {
          lives_decoder_t *dplug;
          sfile->decoder_uid = sfile->old_dec_uid;
          sfile->old_dec_uid = 0;
          //sfile->primary_src = dsource;
          dplug = (lives_decoder_t *)dsource->actor;
          if (dplug) {
            lives_decoder_sys_t *dpsys = (lives_decoder_sys_t *)dplug->dpsys;
            if (!save_clip_value(sclipno, CLIP_DETAILS_DECODER_UID, (void *)&sfile->decoder_uid)) bad_header = TRUE;
            else {
              if (!save_clip_value(sclipno, CLIP_DETAILS_DECODER_NAME, (void *)dpsys->soname)) bad_header = TRUE;
            }
          }
        }
      }
      sfile->clip_type = CLIP_TYPE_FILE;
      if (bad_header) do_header_write_error(sclipno);
    }
    save_frame_index(sclipno);
  } else {
    del_frame_index(sclipno);
    sfile->clip_type = CLIP_TYPE_DISK;
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
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

  if (!sfile) return;

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return;
  }
  for (frames_t i = oldsframe; i <= oldframes; i++) {
    threaded_dialog_spin(0.);
    if ((i <= sfile->frames && sfile->frame_index[i - 1] != -1) || i > sfile->frames) {
      iname = make_image_file_name(sfile, i, get_image_ext_for_type(sfile->img_type));
      lives_rm(iname);
    }
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
}


frames_t *frame_index_copy(frames_t *findex, frames_t nframes, frames_t offset) {
  // copy first nframes from findex and return them, adding offset to each value
  // no checking is done to make sure nframes is in range
  // frame_index_mutex should be locked for src and dest

  // start at frame offset
  frames_t *findexc = (frames_t *)lives_calloc_align(nframes * sizeof(frames_t));
  if (!offset) lives_memcpy((void *)findexc, (void *)findex, nframes * sizeof(frames_t));
  else for (int i = 0; i < nframes; i++) findexc[i] = findex[i + offset];
  return findexc;
}


boolean is_virtual_frame(int sclipno, frames_t frame) {
  // frame is virtual if it is still inside a video clip (read only)
  // once a frame is on disk as an image it is no longer virtual

  // frame starts at 1 here

  // a CLIP_TYPE_FILE with no virtual frames becomes a CLIP_TYPE_DISK

  lives_clip_t *sfile;
  if (!IS_PHYSICAL_CLIP(sclipno)) return FALSE;

  sfile = mainw->files[sclipno];
  if (frame < 1 || frame > sfile->frames) return FALSE;

  pthread_mutex_lock(&sfile->frame_index_mutex);
  if (!sfile->frame_index) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return FALSE;
  }
  if (sfile->frame_index[frame - 1] != -1) {
    pthread_mutex_unlock(&sfile->frame_index_mutex);
    return TRUE;
  }
  pthread_mutex_unlock(&sfile->frame_index_mutex);
  return FALSE;
}


void insert_blank_frames(int sclipno, frames_t nframes, frames_t after, int palette) {
  // insert blank frames in clip (only valid just after clip is opened)

  // this is ugly, it should be moved to another file

  lives_clip_t *sfile;
  LiVESPixbuf *blankp = NULL;
  LiVESError *error = NULL;
  char oname[PATH_MAX];
  char nname[PATH_MAX];
  char *tmp;

  frames_t i;

  if (!IS_PHYSICAL_CLIP(sclipno)) return;
  sfile = mainw->files[sclipno];

  pthread_mutex_lock(&sfile->frame_index_mutex);
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
          pthread_mutex_unlock(&sfile->frame_index_mutex);
          return;
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  for (i = after; i < after + nframes; i++) {
    tmp = make_image_file_name(sfile, i + 1, get_image_ext_for_type(sfile->img_type));
    lives_snprintf(oname, PATH_MAX, "%s", tmp);
    lives_free(tmp);
    if (!blankp) blankp = lives_pixbuf_new_blank(sfile->hsize, sfile->vsize, palette);
    pixbuf_to_png(blankp, oname, sfile->img_type, 100 - prefs->ocp, sfile->hsize, sfile->vsize, &error);
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
    insert_images_in_virtual(sclipno, after, nframes, NULL, 0);

  sfile->frames += nframes;
  pthread_mutex_unlock(&sfile->frame_index_mutex);

  if (blankp) lives_widget_object_unref(blankp);
}
