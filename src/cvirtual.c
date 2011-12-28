// cvirtual.c
// LiVES
// (c) G. Finch 2008 - 2011 <salsaman@xs4all.nl,salsaman@gmail.com>
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
LIVES_INLINE gint count_virtual_frames(int *findex, int start, int end) {
  register int i;
  gint count=0;
  for (i=start-1;i<end;i++) if (findex[i]!=-1) count++;
  return count;
}


void create_frame_index(gint fileno, gboolean init, gint start_offset, gint nframes) {
  register int i;
  file *sfile=mainw->files[fileno];
  if (sfile==NULL||sfile->frame_index!=NULL) return;

  sfile->frame_index=(int *)g_malloc(nframes*sizint);

  if (init) {
    for (i=0;i<sfile->frames;i++) {
      sfile->frame_index[i]=i+start_offset;
    }
  }
}


// save frame_index to disk
gboolean save_frame_index(gint fileno) {
  int fd,i;
  int retval;
  gchar *fname;
  file *sfile=mainw->files[fileno];

  if (sfile==NULL||sfile->frame_index==NULL) return FALSE;

  fname=g_build_filename(prefs->tmpdir,sfile->handle,"file_index",NULL);

  do {
    retval=0;
    fd=open(fname,O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (fd<0) {
      retval=do_write_failed_error_s_with_retry(fname,g_strerror(errno),NULL);
    }
    else {
      mainw->write_failed=FALSE;
      for (i=0;i<sfile->frames;i++) {
	lives_write(fd,&sfile->frame_index[i],sizint,TRUE);
	if (mainw->write_failed) break;
      }
      
      close(fd);

      if (mainw->write_failed) {
	retval=do_write_failed_error_s_with_retry(fname,NULL,NULL);
      }
    }
  } while (retval==LIVES_RETRY);

  g_free(fname);

  if (retval==LIVES_CANCEL) return FALSE;
  
  return TRUE;
}



// load frame_index from disk
gboolean load_frame_index(gint fileno) {
  int fd,i;
  int retval;
  gchar *fname;
  file *sfile=mainw->files[fileno];

  if (sfile==NULL||sfile->frame_index!=NULL) return FALSE;

  if (sfile->frame_index!=NULL) g_free(sfile->frame_index);
  sfile->frame_index=NULL;

  fname=g_build_filename(prefs->tmpdir,sfile->handle,"file_index",NULL);

  if (!g_file_test(fname,G_FILE_TEST_EXISTS)) {
    g_free(fname);
    return FALSE;
  }


  do {
    retval=0;

    fd=open(fname,O_RDONLY);

    if (fd<0) {
      retval=do_read_failed_error_s_with_retry(fname,g_strerror(errno),NULL);
      if (retval==LIVES_CANCEL) {
	g_free(fname);
	return FALSE;
      }
    }
    else {
      create_frame_index(fileno,FALSE,0,sfile->frames);

      mainw->read_failed=FALSE;
      for (i=0;i<sfile->frames;i++) {
	lives_read(fd,&sfile->frame_index[i],sizint,FALSE);
	if (mainw->read_failed) break;
      }

      close(fd);

      if (mainw->read_failed) {
	mainw->read_failed=FALSE;
	retval=do_read_failed_error_s_with_retry(fname,NULL,NULL);
      }

    }
  } while (retval==LIVES_RETRY);

  g_free(fname);

  return TRUE;
}


void del_frame_index(file *sfile) {
  // physically delete the frame_index for a clip
  // only done once all

  gchar *idxfile=g_build_filename(prefs->tmpdir,sfile->handle,"file_index",NULL);
  gchar *com=g_strdup_printf("/bin/rm -f \"%s\"",idxfile);

  register int i;

  // cannot call check_if_non_virtual() else we end up recursing

  if (sfile->frame_index!=NULL) {
    for (i=1;i<=sfile->frames;i++) {
      if (sfile->frame_index[i-1]!=-1) {
	LIVES_ERROR("deleting frame_index with virtual frames in it !");
	g_free(com);
	g_free(idxfile);
	return;
      }
    }
  }

  lives_system(com,FALSE);
  g_free(com);
  g_free(idxfile);
  if (sfile->frame_index!=NULL) g_free(sfile->frame_index);
  sfile->frame_index=NULL;
}




gboolean check_clip_integrity(file *sfile, const lives_clip_data_t *cdata) {
  int i;
  int empirical_img_type;

  // check clip integrity upon loading

  // check that cached values match with sfile (on disk) values
  // TODO: also check sfile->frame_index to make sure all frames are present


  // return FALSE if we find any omissions/inconsistencies


  // check the image type
  for (i=0;i<sfile->frames;i++) {
    if (sfile->frame_index[i]==-1) {
      // this is a non-virtual frame
      gchar *frame=g_strdup_printf("%s/%s/%08d.png",prefs->tmpdir,sfile->handle,i+1);
      if (g_file_test(frame,G_FILE_TEST_EXISTS)) empirical_img_type=IMG_TYPE_PNG;
      else empirical_img_type=IMG_TYPE_JPEG;
      g_free(frame);
      break;
    }
  }


  if (sfile->img_type==empirical_img_type);
  // and all else are equal

  return TRUE;

  // something mismatched - trust the disk version

  sfile->img_type=empirical_img_type;

  return FALSE;
}




gboolean check_if_non_virtual(gint fileno, gint start, gint end) {
  // check if there are no virtual frames from start to end inclusive in clip fileno

  register int i;
  file *sfile=mainw->files[fileno];
  gboolean bad_header=FALSE;

  if (sfile->clip_type!=CLIP_TYPE_FILE) return TRUE;

  if (sfile->frame_index!=NULL) {
    for (i=1;i<=sfile->frames;i++) {
      if (sfile->frame_index[i-1]!=-1) return FALSE;
    }
  }

  if (start>1 || end<sfile->frames) return TRUE;


  // no virtual frames in entire clip - change to CLIP_TYPE_DISK

  sfile->clip_type=CLIP_TYPE_DISK;
  del_frame_index(sfile);

  if (sfile->interlace!=LIVES_INTERLACE_NONE) {
    sfile->interlace=LIVES_INTERLACE_NONE; // all frames should have been deinterlaced
    sfile->deinterlace=FALSE;
    save_clip_value(fileno,CLIP_DETAILS_INTERLACE,&sfile->interlace);
    if (mainw->com_failed||mainw->write_failed) bad_header=TRUE;

    if (bad_header) do_header_write_error(fileno);
  }

  return TRUE;
}



gboolean virtual_to_images(gint sfileno, gint sframe, gint eframe, gboolean update_progress) {
  // pull frames from a clip to images
  // from sframe to eframe inclusive (first frame is 1)

  // if update_progress, set mainw->msg with number of frames pulled

  // should be threadsafe apart from progress update

  // return FALSE on write error

  register int i;
  file *sfile=mainw->files[sfileno];
  GdkPixbuf *pixbuf;
  GError *error=NULL;
  gchar *oname;
  int retval;

  gint progress=1;

  if (sframe<1) sframe=1;

  for (i=sframe;i<=eframe;i++) {
    if (i>sfile->frames) break;

    if (sfile->frame_index[i-1]>=0) {
      oname=NULL;
      threaded_dialog_spin();

      while (g_main_context_iteration(NULL,FALSE));

      pixbuf=pull_gdk_pixbuf_at_size(sfileno,i,sfile->img_type==IMG_TYPE_JPEG?"jpg":"png",q_gint64((i-1.)/sfile->fps,sfile->fps),sfile->hsize,sfile->vsize,GDK_INTERP_HYPER);
      
      if (sfile->img_type==IMG_TYPE_JPEG) {
	oname=g_strdup_printf("%s/%s/%08d.jpg",prefs->tmpdir,sfile->handle,i);
      }
      else if (sfile->img_type==IMG_TYPE_PNG) {
	oname=g_strdup_printf("%s/%s/%08d.png",prefs->tmpdir,sfile->handle,i);
      }

      do {
	retval=0;
	lives_pixbuf_save (pixbuf, oname, sfile->img_type, 100-prefs->ocp, &error);
	if (error!=NULL) {
	  retval=do_write_failed_error_s_with_retry(oname,error->message,NULL);
	  g_error_free(error);
	  error=NULL;
	}
      } while (retval==LIVES_RETRY);

      if (oname!=NULL) g_free(oname);
      if (pixbuf!=NULL) gdk_pixbuf_unref(pixbuf);
      pixbuf=NULL;

      if (retval==LIVES_CANCEL) return FALSE;


      // another thread may have called check_if_non_virtual - TODO : use a mutex
      if (sfile->frame_index==NULL) break;
      sfile->frame_index[i-1]=-1;

      if (update_progress) {
	// sig_progress...
	g_snprintf (mainw->msg,256,"%d",progress++);
	while (g_main_context_iteration(NULL,FALSE));
      }

      threaded_dialog_spin();

      if (mainw->cancelled!=CANCEL_NONE) {
	if (!check_if_non_virtual(sfileno,1,sfile->frames)) save_frame_index(sfileno);
	return TRUE;
      }
    }
  }

  if (!check_if_non_virtual(sfileno,1,sfile->frames)) if (!save_frame_index(sfileno)) return FALSE;

  return TRUE;
}




void insert_images_in_virtual (gint sfileno, gint where, gint frames) {
  // insert physical (frames) images into sfile at position where [0 = before first frame]
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  register int i;
  file *sfile=mainw->files[sfileno];
  int nframes=sfile->frames;

  if (sfile->frame_index_back!=NULL) g_free(sfile->frame_index_back);

  sfile->frame_index_back=sfile->frame_index;
  sfile->frame_index=NULL;

  create_frame_index(sfileno,FALSE,0,nframes+frames);

  for (i=nframes-1;i>=where;i--) {
    sfile->frame_index[i+frames]=sfile->frame_index_back[i];
  }

  for (i=where;i<where+frames;i++) {
    sfile->frame_index[i]=-1;
  }

  for (i=0;i<where;i++) {
    sfile->frame_index[i]=sfile->frame_index_back[i];
  }
  save_frame_index(sfileno);
}




void delete_frames_from_virtual (gint sfileno, gint start, gint end) {
  // delete (frames) images from sfile at position start to end
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  register int i;
  file *sfile=mainw->files[sfileno];
  int nframes=sfile->frames,frames=end-start+1;

  if (sfile->frame_index_back!=NULL) g_free(sfile->frame_index_back);

  sfile->frame_index_back=sfile->frame_index;
  sfile->frame_index=NULL;

  if (nframes-frames==0) {
    del_frame_index(sfile);
    return;
  }

  create_frame_index(sfileno,FALSE,0,nframes-frames);

  for (i=0;i<start-1;i++) {
    sfile->frame_index[i]=sfile->frame_index_back[i];
  }

  for (i=end;i<nframes;i++) {
    sfile->frame_index[i-frames]=sfile->frame_index_back[i];
  }
  save_frame_index(sfileno);
}




void restore_frame_index_back (gint sfileno) {
  // undo an operation
  // this is the virtual (book-keeping) part

  // need to update the frame_index

  // this is for clip type CLIP_TYPE_FILE only

  file *sfile=mainw->files[sfileno];

  if (sfile->frame_index!=NULL) g_free(sfile->frame_index);

  sfile->frame_index=sfile->frame_index_back;
  sfile->frame_index_back=NULL;

  if (sfile->frame_index!=NULL) {
    sfile->clip_type=CLIP_TYPE_FILE;
    save_frame_index(sfileno);
  }
  else {
    del_frame_index(sfile);
    sfile->clip_type=CLIP_TYPE_DISK;
  }
}




void clean_images_from_virtual (file *sfile, gint oldframes) {
  // remove images on disk where the frame_index points to a frame in
  // the original clip

  // only needed if frames were reordered when rendered and the process is
  // then undone

  // in future, a smarter function could trace the images back to their
  // original source frames, and just rename them


  // should be threadsafe

  register int i;
  gchar *iname=NULL,*com;

  if (sfile==NULL||sfile->frame_index==NULL) return;

  for (i=0;i<oldframes;i++) {
    threaded_dialog_spin();
    while (g_main_context_iteration(NULL,FALSE));
    threaded_dialog_spin();

    if ((i<sfile->frames&&sfile->frame_index[i]!=-1)||i>=sfile->frames) {
      if (sfile->img_type==IMG_TYPE_JPEG) {
	iname=g_strdup_printf("%s/%s/%08d.jpg",prefs->tmpdir,sfile->handle,i);
      }
      else if (sfile->img_type==IMG_TYPE_PNG) {
	iname=g_strdup_printf("%s/%s/%08d.png",prefs->tmpdir,sfile->handle,i);
      }
      //      else {
	// ...
      //}
      com=g_strdup_printf("/bin/rm -f \"%s\"",iname);
      lives_system(com,FALSE);
      g_free(com);
    }
  }
}


int *frame_index_copy(int *findex, gint nframes) {
  // like it says on the label
  // copy first nframes from findex and return them
  // no checking is done to make sure nframes is in range

  int *findexc=(int *)g_malloc(sizint*nframes);
  register int i;

  for (i=0;i<nframes;i++) findexc[i]=findex[i];

  return findexc;
}


gboolean is_virtual_frame(int sfileno, int frame) {
  // frame is virtual if it is still inside a video clip (read only)
  // once a frame is on disk as an image it is no longer virtual

  // frame starts at 1 here

  // a CLIP_TYPE_FILE with no virtual frames becomes a CLIP_TYPE_DISK

  file *sfile=mainw->files[sfileno];
  if (sfile->frame_index==NULL) return FALSE;
  if (sfile->frame_index[frame-1]!=-1) return TRUE;
  return FALSE;
}
