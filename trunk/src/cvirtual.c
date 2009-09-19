// cvirtual.c
// LiVES
// (c) G. Finch 2008 - 2009 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions for handling "virtual" clips (CLIP_TYPE_FILE)


#include "main.h"

// frame_index is for files of type CLIP_TYPE_FILE
// a positive number is a pointer to a frame within the video file
// -1 means frame is stored as the corresponding image file
// e.g 00000001.jpg or 00000010.png etc.

#include "resample.h"



LIVES_INLINE gint count_virtual_frames(int *findex, int size) {
  register int i;
  gint count=0;
  for (i=0;i<size;i++) if (findex[i]!=-1) count++;
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
  gchar *fname;
  file *sfile=mainw->files[fileno];
  if (sfile==NULL||sfile->frame_index==NULL) return FALSE;

  fname=g_strdup_printf("%s/%s/file_index",prefs->tmpdir,sfile->handle);
  fd=open(fname,O_CREAT|O_WRONLY|O_TRUNC,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
  g_free(fname);

  if (fd<0) {
    g_printerr("\n\nfailed to open file_index\n");
    return FALSE;
  }

  for (i=0;i<sfile->frames;i++) {
    dummyvar=write(fd,&sfile->frame_index[i],sizint);
  }

  close(fd);

  return TRUE;
}



// save frame_index to disk
gboolean load_frame_index(gint fileno) {
  int fd,i;
  gchar *fname;
  file *sfile=mainw->files[fileno];
  if (sfile==NULL||sfile->frame_index!=NULL) return FALSE;

  if (sfile->frame_index!=NULL) g_free(sfile->frame_index);
  sfile->frame_index=NULL;

  fname=g_strdup_printf("%s/%s/file_index",prefs->tmpdir,sfile->handle);
  fd=open(fname,O_RDONLY);
  g_free(fname);

  if (fd<0) {
    return FALSE;
  }

  create_frame_index(fileno,FALSE,0,sfile->frames);

  for (i=0;i<sfile->frames;i++) {
    dummyvar=read(fd,&sfile->frame_index[i],sizint);
  }

  close(fd);
  return TRUE;
}


void del_frame_index(file *sfile) {
  gchar *com=g_strdup_printf("/bin/rm -f %s/%s/file_index",prefs->tmpdir,sfile->handle);
  dummyvar=system(com);
  g_free(com);
  if (sfile->frame_index!=NULL) g_free(sfile->frame_index);
  sfile->frame_index=NULL;
}




gboolean check_clip_integrity(file *sfile, const lives_clip_data_t *cdata) {
  // check that cdata values match with sfile values
  // also check sfile->frame_index to make sure all frames are present


  // return FALSE if we find any omissions/inconsistencies

  // TODO ***

  return TRUE;

}




gboolean check_if_non_virtual(file *sfile) {
  register int i;

  if (sfile->frame_index!=NULL) {
    for (i=1;i<=sfile->frames;i++) {
      if (sfile->frame_index[i-1]!=-1) return FALSE;
    }
  }

  sfile->clip_type=CLIP_TYPE_DISK;
  del_frame_index(sfile);

  return TRUE;
}



void virtual_to_images(gint sfileno, gint sframe, gint eframe) {
  // pull frames from a clip to images
  // from sframe to eframe inclusive (first frame is 1)

  // should be threadsafe

  register int i;
  file *sfile=mainw->files[sfileno];
  GdkPixbuf *pixbuf;
  GError *error=NULL;
  gchar *oname;

  if (sframe<1) sframe=1;

  for (i=sframe;i<=eframe;i++) {
    if (i>sfile->frames) break;

    pthread_mutex_lock(&mainw->gtk_mutex);
    if (sfile->frame_index[i-1]>=0) {

      while (g_main_context_iteration(NULL,FALSE));
    
      pixbuf=pull_gdk_pixbuf_at_size(sfileno,i,NULL,q_gint64((i-1.)/sfile->fps,sfile->fps),sfile->hsize,sfile->vsize,GDK_INTERP_HYPER);
      
      if (!strcmp (prefs->image_ext,"jpg")) {
	gchar *qstr=g_strdup_printf("%d",(100-prefs->ocp));
	oname=g_strdup_printf("%s/%s/%08d.jpg",prefs->tmpdir,sfile->handle,i);
	gdk_pixbuf_save (pixbuf, oname, "jpeg", &error,"quality", qstr, NULL);
	g_free(qstr);
	g_free(oname);
      }
      else if (!strcmp (prefs->image_ext,"png")) {
	gchar *cstr=g_strdup_printf("%d",(gint)((gdouble)(prefs->ocp+5.)/10.));
	oname=g_strdup_printf("%s/%s/%08d.png",prefs->tmpdir,sfile->handle,i);
	gdk_pixbuf_save (pixbuf, oname, "png", &error, "compression", "cstr", NULL);
	g_free(cstr);
	g_free(oname);
      }
      else {
	//gdk_pixbuf_save_to_callback(...);
      }

      if (error!=NULL) g_printerr("err was %s\n",error->message);

      if (pixbuf!=NULL) gdk_pixbuf_unref(pixbuf);
      pixbuf=NULL;
      sfile->frame_index[i-1]=-1;
    }
    pthread_mutex_unlock(&mainw->gtk_mutex);

    if (mainw->cancelled!=CANCEL_NONE) {
      if (!check_if_non_virtual(sfile)) save_frame_index(sfileno);
      return;
    }
  }

  if (!check_if_non_virtual(sfile)) save_frame_index(sfileno);
}




void insert_images_in_virtual (gint sfileno, gint where, gint frames) {
  // insert (frames) images into sfile at position where [0 = before first frame]
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
    pthread_mutex_lock(&mainw->gtk_mutex);
    while (g_main_context_iteration(NULL,FALSE));
    pthread_mutex_unlock(&mainw->gtk_mutex);

    if ((i<sfile->frames&&sfile->frame_index[i]!=-1)||i>=sfile->frames) {
      if (!strcmp (prefs->image_ext,"jpg")) {
	iname=g_strdup_printf("%s/%s/%08d.jpg",prefs->tmpdir,sfile->handle,i);
      }
      else if (!strcmp (prefs->image_ext,"png")) {
	iname=g_strdup_printf("%s/%s/%08d.png",prefs->tmpdir,sfile->handle,i);
      }
      //      else {
	// ...
      //}
      com=g_strdup_printf("/bin/rm -f %s",iname);
      dummyvar=system(com);
      g_free(com);
    }
  }
}


int *frame_index_copy(int *findex, gint nframes) {
  int *findexc=(int *)malloc(sizint*nframes);
  register int i;

  for (i=0;i<nframes;i++) findexc[i]=findex[i];

  return findexc;
}
