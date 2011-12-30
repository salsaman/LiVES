// stream.c
// LiVES
// (c) G. Finch 2008 - 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// TODO - implement multicast streaming

#include "main.h"
#include "stream.h"
#include "htmsocket.h"
#include "support.h"

#ifdef HAVE_SYSTEM_WEED
#include "weed/weed.h"
#include "weed/weed-host.h"
#include "weed/weed-palettes.h"
#else
#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-palettes.h"
#endif

#define L2L_PACKET_LEN 1024

static gchar pckbuf[L2L_PACKET_LEN*2];
static int pckoffs=0;
static size_t pcksize=0;

static gboolean has_last_delta_ticks;
static gchar *hdr=NULL;
static gboolean fps_can_change;

static inline gint64 abs64(gint64 a) {
    return ((a>0)?a:-a);
}

//#define USE_STRMBUF
#ifdef USE_STRMBUF

#define STREAM_BUF_SIZE 1024*1024*128 // allow 8MB buffer size; actually we use twice this much: - TODO - make pref
static volatile gboolean buffering;

void *streambuf (void *arg) {
  // read bytes from udp port and load into ringbuffer
  ssize_t res; 
  size_t btowrite;
  guchar tmpbuf[STREAM_BUF_SIZE];
  size_t tmpbufoffs;
  lives_vstream_t *lstream=(lives_vstream_t *)arg;

  lstream->bufoffs=0;

  while (buffering) {
    // keep calling lives_stream_in until main thread sets buffering to FALSE
    res=lives_stream_in(lstream->handle,STREAM_BUF_SIZE,tmpbuf,0);
    if (res>0) {
      // got packet of size res
      tmpbufoffs=0;
      btowrite=res;
      if (lstream->bufoffs+btowrite>STREAM_BUF_SIZE) {
	btowrite=STREAM_BUF_SIZE-lstream->bufoffs;
	w_memcpy(lstream->buffer+lstream->bufoffs,tmpbuf,btowrite);
	tmpbufoffs+=btowrite;
	res-=btowrite;
	lstream->bufoffs=0;
      }
      w_memcpy(lstream->buffer+lstream->bufoffs,tmpbuf+tmpbufoffs,res);
      lstream->bufoffs+=res;
      lstream->reading=TRUE;
    }
    //else lstream->reading=FALSE;
  }
  pthread_exit(NULL);
  return NULL;
}



static size_t l2l_rcv_packet(lives_vstream_t *lstream, size_t buflen, void *buf) {
  // take a packet from our stream buffer
  static size_t bufoffs=0;
  size_t btoread=0;
  size_t end=bufoffs+buflen;

  while ((end<STREAM_BUF_SIZE&&lstream->bufoffs>=bufoffs&&lstream->bufoffs<=end)||(end>=STREAM_BUF_SIZE&&(lstream->bufoffs>=bufoffs||lstream->bufoffs<=(end-STREAM_BUF_SIZE)))) {
    g_usleep(1000);
  }

  while (1) {
    // loop until we read the packet, or the user cancels
    if (lstream->reading) {
      if (buflen+bufoffs>STREAM_BUF_SIZE) {
	btoread=STREAM_BUF_SIZE-bufoffs;
	w_memcpy(buf,(void *)((guchar *)lstream->buffer+bufoffs),btoread);
	bufoffs=0;
	buflen-=btoread;
	buf=(void *)((guchar *)buf+btoread);
      }
      w_memcpy(buf,(void *)((guchar *)lstream->buffer+bufoffs),buflen);
      bufoffs+=buflen;
      return buflen+btoread;
    }
    else {
      weed_plant_t *frame_layer=mainw->frame_layer;
      mainw->frame_layer=NULL;
      while (g_main_context_iteration(NULL,FALSE));
      mainw->frame_layer=frame_layer;
      threaded_dialog_spin();
      if (mainw->cancelled) return buflen+btoread;
      g_usleep(prefs->sleep_time);
    }
  }
}


static gboolean lives_stream_in_chunks(lives_vstream_t *lstream, size_t buflen, guchar *buf, int xx) {
  // read first from pckbuf, then from streambuf
  size_t copied=0;
  if (pckoffs<L2L_PACKET_LEN) {
    // use up our pckbuf
    copied=L2L_PACKET_LEN-pckoffs;
    if (copied>buflen) copied=buflen;
    
    w_memcpy(buf,pckbuf,copied);

    buflen-=copied;
    pckoffs+=copied;
  }
  if (buflen>0) l2l_rcv_packet(lstream,buflen,(void *)((guchar *)buf+copied));
  return TRUE;
}
#else


static size_t l2l_rcv_packet(lives_vstream_t *lstream, size_t buflen, void *buf) {
  int ret;
  do {
    ret=lives_stream_in(lstream->handle,buflen,buf,0);
    if (ret==-1) {
      weed_plant_t *frame_layer=mainw->frame_layer;
      mainw->frame_layer=NULL;
      while (g_main_context_iteration(NULL,FALSE));
      mainw->frame_layer=frame_layer;
      threaded_dialog_spin();
      if (mainw->cancelled) {
	return -1;
      }
      g_usleep(prefs->sleep_time);
    }
  } while (ret==-1);
  return ret;
}


static gboolean lives_stream_in_chunks(lives_vstream_t *lstream, size_t buflen, guchar *buf, int bfsize) {
  // return FALSE if we could not set socket buffer size

  size_t copied;
 
  if (pckoffs<pcksize) {
    // use up our pckbuf
    copied=pcksize-pckoffs;
    if (copied>buflen) copied=buflen;
    
    w_memcpy(buf,pckbuf,copied);
    
    buflen-=copied;
    pckoffs+=copied;
    buf+=copied;
  }
  while (buflen>0) {
    // read in the rest
    do {
      copied=lives_stream_in(lstream->handle,buflen,buf,bfsize);
      if (copied==-2) return FALSE;
      if (copied==-1) {
	weed_plant_t *frame_layer=mainw->frame_layer;
	mainw->frame_layer=NULL;
	while (g_main_context_iteration(NULL,FALSE));
	mainw->frame_layer=frame_layer;
	threaded_dialog_spin();
	if (mainw->cancelled) return TRUE;
	g_usleep(prefs->sleep_time);
      }
    } while (copied==-1);
    buflen-=copied;
    buf+=copied;
  }
  return TRUE;
}

#endif

static void l2l_get_packet_sync(lives_vstream_t *lstream) {
  gboolean sync=FALSE;

  if (pckoffs==pcksize) {
    pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
    pckoffs=0;
    if (mainw->cancelled) return;
  }

  while (!sync) {
    while (strncmp(pckbuf+pckoffs,"P",1)) {
      if (++pckoffs==pcksize) {
	pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
	pckoffs=0;
	if (mainw->cancelled) return;
      }
    }
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs,"A",1)) continue;
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs,"C",1)) continue;
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs,"K",1)) continue;
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs,"E",1)) continue;
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs,"T",1)) continue;
    if (++pckoffs==pcksize) {
      pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
      pckoffs=0;
      if (mainw->cancelled) return;
    }
    if (strncmp(pckbuf+pckoffs," ",1)) continue;

    sync=TRUE;
  }
  pckoffs++;
}


static gchar *l2l_get_packet_header(lives_vstream_t *lstream) {
  gchar hdr_buf[1024];
  gboolean sync=FALSE;
  size_t hdrsize=0,csize;

  if (pckoffs==pcksize) {
    pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
    if (mainw->cancelled) return NULL;
  }
  
  while (!sync) {
    if (pckoffs==pcksize) {
      if (pcksize>L2L_PACKET_LEN) {
	csize=pcksize;
	pcksize=(pcksize+1)>>1;
	csize-=pcksize;
	w_memcpy(pckbuf,pckbuf+pcksize,csize);
	pckoffs-=pcksize;
	pcksize=csize;
      }
      pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
      if (mainw->cancelled) return NULL;
    }

    while (strncmp(pckbuf+pckoffs,"D",1)) {
      hdr_buf[hdrsize++]=pckbuf[pckoffs];
      if (hdrsize>1000) {
	if (pckoffs>=L2L_PACKET_LEN) {
	  w_memcpy(pckbuf,pckbuf+L2L_PACKET_LEN,L2L_PACKET_LEN);
	  pckoffs-=L2L_PACKET_LEN;
	}
	return NULL;
      }
      if (++pckoffs==pcksize) {
	if (pcksize>L2L_PACKET_LEN) {
	  csize=pcksize;
	  pcksize=(pcksize+1)>>1;
	  csize-=pcksize;
	  w_memcpy(pckbuf,pckbuf+pcksize,csize);
	  pckoffs-=pcksize;
	  pcksize=csize;
	}
	pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
	if (mainw->cancelled) return NULL;
      }
    }

    if (++pckoffs==pcksize) {
      if (pcksize>L2L_PACKET_LEN) {
	csize=pcksize;
	pcksize=(pcksize+1)>>1;
	csize-=pcksize;
	w_memcpy(pckbuf,pckbuf+pcksize,csize);
	pckoffs-=pcksize;
	pcksize=csize;
      }
      pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
      if (mainw->cancelled) return NULL;
    }

    if (strncmp(pckbuf+pckoffs,"A",1)) {
      w_memcpy (&hdr_buf[hdrsize],pckbuf+pckoffs-1,2);
      hdrsize+=2;
      continue;
    }

    if (++pckoffs==pcksize) {
      if (pcksize>L2L_PACKET_LEN) {
	csize=pcksize;
	pcksize=(pcksize+1)>>1;
	csize-=pcksize;
	w_memcpy(pckbuf,pckbuf+pcksize,csize);
	pckoffs-=pcksize;
	pcksize=csize;
      }
      pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
      if (mainw->cancelled) return NULL;
    }

    if (strncmp(pckbuf+pckoffs,"T",1)) {
      w_memcpy (&hdr_buf[hdrsize],pckbuf+pckoffs-2,3);
      hdrsize+=3;
      continue;
    }

    if (++pckoffs==pcksize) {
      if (pcksize>L2L_PACKET_LEN) {
	csize=pcksize;
	pcksize=(pcksize+1)>>1;
	csize-=pcksize;
	w_memcpy(pckbuf,pckbuf+pcksize,csize);
	pckoffs-=pcksize;
	pcksize=csize;
      }
      pcksize+=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf+pckoffs);
      if (mainw->cancelled) return NULL;
    }

    if (strncmp(pckbuf+pckoffs,"A",1)) {
      w_memcpy (&hdr_buf[hdrsize],pckbuf+pckoffs-3,4);
      hdrsize+=4;
      continue;
    }
    sync=TRUE;
  }

  if (pckoffs>=L2L_PACKET_LEN) {
    w_memcpy(pckbuf,pckbuf+L2L_PACKET_LEN,L2L_PACKET_LEN);
    pckoffs-=L2L_PACKET_LEN;
    pcksize-=L2L_PACKET_LEN;
  }

  hdr_buf[hdrsize]=0;

  pckoffs++;

  return g_strdup(hdr_buf);
}


static gboolean l2l_parse_packet_header(lives_vstream_t *lstream, gint strtype, gint strid) {
  gchar **array=g_strsplit(hdr," ",-1);
  gint pid=-1,ptype=-1;
  
  if (hdr==NULL||array==NULL||array[0]==NULL||array[1]==NULL||array[2]==NULL||array[3]==NULL) {
    if (array!=NULL) g_strfreev(array);
    return FALSE;
  }

  ptype=atoi(array[0]);
  pid=atoi(array[1]);

  if (pid!=strid||ptype!=strtype) {
    // wrong stream id, or not video
    if (array!=NULL) g_strfreev(array);
    return FALSE;
  }

  lstream->flags=atoi(array[2]);
  lstream->dsize=atoi(array[3]);

  if (!(lstream->flags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION)) {
    if (capable->cpu_bits==32) {
      lstream->timecode=strtoll(array[4],NULL,10);
    }
    else {
      lstream->timecode=strtol(array[4],NULL,10);
    }

    lstream->hsize=atoi(array[5]);
    lstream->vsize=atoi(array[6]);
    lstream->fps=g_strtod(array[7],NULL);
    lstream->palette=atoi(array[8]);
    lstream->YUV_sampling=atoi(array[9]);
    lstream->YUV_clamping=atoi(array[10]);
    lstream->YUV_subspace=atoi(array[11]);
    lstream->compression_type=atoi(array[12]);
  }

  g_strfreev(array);
  lstream->data_ready=TRUE;

  return TRUE;
}



void lives2lives_read_stream(const gchar *host, int port) {
  lives_vstream_t *lstream=(lives_vstream_t *)g_malloc(sizeof(lives_vstream_t));
  gboolean done=FALSE;
  gint old_file=mainw->current_file,new_file;
  gchar *tmp,*tmp2;
  gchar *msg;
  gchar *hostname;

#ifdef USE_STRMBUF
  pthread_t stthread;
  pthread_attr_t pattr;
#endif

  gtk_widget_set_sensitive (mainw->open_lives2lives, FALSE);

  lstream->handle=OpenHTMSocket(host,port,FALSE);
  if (lstream->handle==NULL) {
    do_error_dialog(_("LiVES to LiVES stream error: Could not open port !\n"));
    gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
    return;
  }

  msg=g_strdup_printf(_("Waiting for LiVES stream on port %d..."),port);
  d_print(msg);
  g_free(msg);

  mainw->cancelled=CANCEL_NONE;
  do_threaded_dialog(_("\nWaiting for stream"),TRUE);

#ifdef USE_STRMBUF
  buffering=TRUE;
  lstream->reading=FALSE;

  pthread_attr_init(&pattr);
  if (pthread_attr_setstacksize(&pattr,STREAM_BUF_SIZE*4)) {
    do_error_dialog(_("LiVES to LiVES stream error: Could not set buffer size !\n"));
    g_free(lstream);
    gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
    return;
  }
  lstream->buffer=g_malloc(STREAM_BUF_SIZE);
  pthread_create(&stthread,&pattr,streambuf,(void *)lstream);
  pthread_attr_destroy(&pattr);
#endif

  pcksize=l2l_rcv_packet(lstream, L2L_PACKET_LEN, pckbuf);
  pckoffs=0;
  if (mainw->cancelled) {
    end_threaded_dialog();
#ifdef USE_STRMBUF
    buffering=FALSE;
    pthread_join(stthread,NULL);
#endif
    CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
    g_free(lstream->buffer);
#endif
    g_free(lstream);
    d_print_cancelled();
    gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
    return;
  }

  while (!done) {
    do {
      // get video stream 0 PACKET
      l2l_get_packet_sync(lstream);
      if (mainw->cancelled) {
	end_threaded_dialog();
#ifdef USE_STRMBUF
	buffering=FALSE;
	pthread_join(stthread,NULL);
#endif
	CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
	g_free(lstream->buffer);
#endif
	g_free(lstream);
	d_print_cancelled();
	gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
	return;
      }
      // get packet header
      hdr=l2l_get_packet_header(lstream);
      if (mainw->cancelled) {
	end_threaded_dialog();
#ifdef USE_STRMBUF
	buffering=FALSE;
	pthread_join(stthread,NULL);
#endif
	CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
	g_free(lstream->buffer);
#endif
	g_free(lstream);
	d_print_cancelled();
	gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
	return;
      }
    } while (hdr==NULL);
    // parse packet header
    done=l2l_parse_packet_header(lstream,LIVES_STREAM_TYPE_VIDEO,0);
    if (lstream->flags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION) done=FALSE;
    if (!done) {
      // wrong packet type or id, or a continuation packet
      guchar *tmpbuf=g_malloc(lstream->dsize);
      lives_stream_in_chunks(lstream,lstream->dsize,tmpbuf,lstream->dsize*4);
      // throw this packet away
      g_printerr("unrecognised packet in stream - dropping it.\n");
      g_free(tmpbuf);
      if (mainw->cancelled) {
	end_threaded_dialog();
#ifdef USE_STRMBUF
	buffering=FALSE;
	pthread_join(stthread,NULL);
#endif
	CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
	g_free(lstream->buffer);
#endif
	g_free(lstream);
	d_print_cancelled();
	gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
	return;
      }
    }
    g_free(hdr);
  }

  end_threaded_dialog();

  if (mainw->fixed_fpsd<=0.) fps_can_change=TRUE;
  else fps_can_change=FALSE;

  if (mainw->fixed_fpsd>0.&&(cfile->fps!=mainw->fixed_fpsd)) {
    do_error_dialog (_ ("\n\nUnable to open stream, framerate does not match fixed rate.\n"));
#ifdef USE_STRMBUF
    buffering=FALSE;
    pthread_join(stthread,NULL);
#endif
    CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
    g_free(lstream->buffer);
#endif
    g_free(lstream);
    d_print_failed();
    gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
    return;
  }

  // now we should have lstream details

  //
  new_file=mainw->first_free_file;
  if (!get_new_handle(new_file,"LiVES to LiVES stream")) {
    mainw->error=TRUE;
#ifdef USE_STRMBUF
    buffering=FALSE;
    pthread_join(stthread,NULL);
#endif
    CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
    g_free(lstream->buffer);
#endif
    g_free(lstream);
    d_print_failed();
    gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
    return;
  }

  mainw->current_file=new_file;
  cfile->clip_type=CLIP_TYPE_LIVES2LIVES;

  cfile->fps=lstream->fps;
  cfile->hsize=lstream->hsize;
  cfile->vsize=lstream->vsize;

  cfile->ext_src=lstream;

  switch_to_file((mainw->current_file=old_file),new_file);
  set_main_title(cfile->file_name,0);
  add_to_winmenu();

  cfile->achans=0;
  cfile->asampsize=0;

  // open as a clip with 1 frame
  cfile->start=cfile->end=cfile->frames=1;
  cfile->arps=cfile->arate=0;
  mainw->fixed_fpsd=cfile->pb_fps=cfile->fps;

  cfile->opening=FALSE;
  cfile->proc_ptr=NULL;

  cfile->changed=FALSE;

  // allow clip switching
  cfile->is_loaded=TRUE;

  d_print("\n");

  g_snprintf(cfile->type,40,"LiVES to LiVES stream in");

  if (!strcmp(host,"INADDR_ANY")) hostname=g_strdup(_("any host"));
  else hostname=g_strdup(_("host %d"));

  d_print ((tmp=g_strdup_printf (_("Opened LiVES to LiVES stream from %s on port %d"),hostname,port)));
  g_free(tmp);
  g_free(hostname);
  d_print ((tmp=g_strdup_printf(_ (" size=%dx%d bpp=%d fps=%.3f\nAudio: "),cfile->hsize,cfile->vsize,cfile->bpp,cfile->fps)));
  g_free(tmp);

  if (cfile->achans==0) {
    d_print (_ ("none\n"));
  }
  else {
    d_print ((tmp=g_strdup_printf(_ ("%d Hz %d channel(s) %d bps\n"),cfile->arate,cfile->achans,cfile->asampsize)));
    g_free(tmp);
  }

  d_print ((tmp=g_strdup_printf (_("Syncing to external framerate of %s frames per second.\n"),(tmp2=remove_trailing_zeroes(mainw->fixed_fpsd)))));
  g_free(tmp);
  g_free(tmp2);

  has_last_delta_ticks=FALSE;

  // if not playing, start playing
  if (mainw->playing_file==-1) {
    if (mainw->play_window!=NULL&&old_file==-1) {
      // usually preview or load_preview_frame would do this
      g_signal_handler_block(mainw->play_window,mainw->pw_exp_func);
      mainw->pw_exp_is_blocked=TRUE;
    }
    mainw->play_start=1;
    mainw->play_end=INT_MAX;
    play_file();
    mainw->noswitch=FALSE;
  }
  // TODO - else...
  
  if (mainw->current_file!=old_file&&mainw->current_file!=new_file) old_file=mainw->current_file; // we could have rendered to a new file

  mainw->fixed_fpsd=-1.;
  d_print (_("Sync lock off.\n"));
  mainw->current_file=new_file;
#ifdef USE_STRMBUF
    buffering=FALSE;
    pthread_join(stthread,NULL);
#endif
  CloseHTMSocket(lstream->handle);
#ifdef USE_STRMBUF
    g_free(lstream->buffer);
#endif
  g_free (cfile->ext_src);
  cfile->ext_src=NULL;

  close_current_file(old_file);
  gtk_widget_set_sensitive (mainw->open_lives2lives, TRUE);
}



void weed_layer_set_from_lives2lives(weed_plant_t *layer, gint clip, lives_vstream_t *lstream) {
  static int64_t last_delta_ticks=0;
  int64_t currticks;
  gboolean done;
  int error;
  void **pixel_data;
  int myflags=0;
  size_t framedataread=0;
  gboolean timeout=FALSE; // TODO
  gint width=0;
  gint height=0;
  size_t target_size;

  while (!timeout) {
    // loop until we read all frame data, or we get a new frame
    done=FALSE;
    if (!lstream->data_ready) {
      while (!done) {
	// get video stream 0 PACKET
	do {
	  l2l_get_packet_sync(lstream);
	  if (mainw->cancelled) return;
	  // get packet header
	  hdr=l2l_get_packet_header(lstream);
	  if (mainw->cancelled) return;
	} while (hdr==NULL&&mainw->cancelled==CANCEL_NONE);
	if (mainw->cancelled) {
	  g_free(hdr);
	  hdr=NULL;
	  return;
	}
	// parse packet header
	done=l2l_parse_packet_header(lstream,LIVES_STREAM_TYPE_VIDEO,0);
	if (!(myflags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION)&&(lstream->flags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION)) done=FALSE;
	if ((myflags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION)&&!(lstream->flags&LIVES_VSTREAM_FLAGS_IS_CONTINUATION)) {
	  // we missed some continuation packets, just return what we have
	  lstream->data_ready=TRUE;
	  g_free(hdr);
	  hdr=NULL;
	  return;
	}

	if (!done) {
	  // wrong packet type or id, or a continuation of previous frame
	  guchar *tmpbuf=g_malloc(lstream->dsize);
	  lives_stream_in_chunks(lstream,lstream->dsize,tmpbuf,lstream->dsize*4);
	  // throw this packet away
	  g_printerr("unrecognised packet in stream - dropping it.\n");
	  g_free(tmpbuf);
	  if (mainw->cancelled) {
	    g_free(hdr);
	    hdr=NULL;
	    return;
	  }
	}
	g_free(hdr);
	hdr=NULL;

	if (lstream->fps!=mainw->fixed_fpsd&&fps_can_change) {
	  gchar *tmp,*tmp2;
	  d_print(_("Detected new framerate for stream:\n"));
	  mainw->files[clip]->fps=mainw->fixed_fpsd=lstream->fps;
	  d_print ((tmp=g_strdup_printf (_("Syncing to external framerate of %s frames per second.\n"),(tmp2=remove_trailing_zeroes(mainw->fixed_fpsd)))));
	  g_free(tmp);
	  g_free(tmp2);
	  has_last_delta_ticks=FALSE;
	  if (clip==mainw->current_file) set_main_title(cfile->file_name,0);
	}

#define DROP_AGING_FRAMES
#ifdef DROP_AGING_FRAMES
	// this seems to help smoothing when recording, however I have only tested it on one machine
	// where frames were being generated and streamed and then received
	// - needs testing in other situations
	gettimeofday(&tv, NULL);
	currticks=U_SECL*(tv.tv_sec-mainw->origsecs)+tv.tv_usec*U_SEC_RATIO-mainw->origusecs*U_SEC_RATIO;
	if (mainw->record&&!mainw->record_paused) {
	  if (has_last_delta_ticks&&(abs64(currticks-lstream->timecode))<last_delta_ticks) {
	    // drop this frame
	    guchar *tmpbuf=g_malloc(lstream->dsize);
	    lives_stream_in_chunks(lstream,lstream->dsize,tmpbuf,lstream->dsize*4);
	    // throw this packet away
#ifdef DEBUG_STREAM_AGING
	    g_printerr("packet too early (!) - dropping it.\n");
#endif
	    g_free(tmpbuf);
	    done=FALSE;
	    if (mainw->cancelled) {
	      g_free(hdr);
	      hdr=NULL;
	      return;
	    }
	  }
	}
	last_delta_ticks=((gint64)(last_delta_ticks>>1)+(gint64)((abs64(currticks-lstream->timecode))>>1));
#endif

      }
    }

    if (!has_last_delta_ticks) {
      last_delta_ticks=abs64(mainw->currticks-lstream->timecode);
    }
    has_last_delta_ticks=TRUE;

    lstream->data_ready=FALSE;

    width=mainw->files[clip]->hsize;
    height=mainw->files[clip]->vsize;


    if (lstream->hsize!=width||lstream->vsize!=height) {
      // frame size changed...
      gchar *msg=g_strdup_printf((_("Detected frame size change to %d x %d\n")),lstream->hsize,lstream->vsize);
      d_print(msg);
      g_free(msg);
      
      mainw->files[clip]->hsize=lstream->hsize;
      mainw->files[clip]->vsize=lstream->vsize;

      if (clip==mainw->current_file) {
	set_main_title(cfile->file_name,0);
      }
      frame_size_update();
    }

    width=height=0;

    if (weed_plant_has_leaf(layer,"height")) height=weed_get_int_value(layer,"height",&error);
    if (weed_plant_has_leaf(layer,"width")) width=weed_get_int_value(layer,"width",&error);

    if (lstream->hsize!=width||lstream->vsize!=height) {
      if (weed_plant_has_leaf(layer,"pixel_data")) {
	// ...free old pixel_data
	int i,np=weed_leaf_num_elements(layer,"pixel_data");
	pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
	if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") && 
	    weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
	  np=1;
	for (i=0;i<np;i++) {
	  g_free(pixel_data[i]);
	}
	weed_free(pixel_data);
	weed_leaf_delete(layer,"pixel_data");
      }
    }


    if (!weed_plant_has_leaf(layer,"pixel_data")) {
      weed_set_int_value(layer,"width",lstream->hsize);
      weed_set_int_value(layer,"height",lstream->vsize);
      weed_set_int_value(layer,"current_palette",lstream->palette);
      weed_set_int_value(layer,"YUV_clamping",lstream->YUV_clamping);
      create_empty_pixel_data(layer,FALSE,TRUE);
    }

    pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

    switch (lstream->palette) {
    case WEED_PALETTE_RGB24:
      target_size=lstream->hsize*lstream->vsize*3-framedataread;
#ifdef USE_STRMBUF
      if (target_size>lstream->dsize) target_size=lstream->dsize;
      lives_stream_in_chunks(lstream,target_size,(guchar *)pixel_data[0]+framedataread,0);
      lstream->dsize-=target_size;
      framedataread+=target_size;
#else
      if (target_size>=lstream->dsize) {
	if (!lives_stream_in_chunks(lstream,target_size,(guchar *)pixel_data[0]+framedataread,lstream->dsize*12)) {
	  do_rmem_max_error(lstream->dsize*12);
	  mainw->cancelled=CANCEL_ERROR;
	}
	if (mainw->cancelled) {
	  weed_free(pixel_data);
	  return;
	}
      }
#endif
      weed_free(pixel_data);
      if (framedataread>=lstream->hsize*lstream->vsize*3) {
	return;
      }
      myflags|=LIVES_VSTREAM_FLAGS_IS_CONTINUATION;
      break;
    case WEED_PALETTE_YUV420P:
      // assume uncompressed, - TODO

      if (framedataread<lstream->hsize*lstream->vsize) {
	target_size=lstream->hsize*lstream->vsize-framedataread;
#ifdef USE_STRMBUF
	if (target_size>lstream->dsize) target_size=lstream->dsize;
	lives_stream_in_chunks(lstream,target_size,(guchar *)pixel_data[0]+framedataread,0);
	lstream->dsize-=target_size;
	framedataread+=target_size;
#else
	if (target_size>=lstream->dsize) {
	  // packet contains data for single plane
	  if (!lives_stream_in_chunks(lstream,lstream->dsize,(guchar *)pixel_data[0]+framedataread,lstream->dsize*9)) {
	    do_rmem_max_error(lstream->dsize*9);
	    mainw->cancelled=CANCEL_ERROR;
	  }
	  if (mainw->cancelled) {
	    weed_free(pixel_data);
	    return;
	  }
	}
	else {
	  // this packet contains data for multiple planes
	  guchar *fbuffer=g_malloc(lstream->dsize);
	  size_t fbufoffs=0;
	  size_t dsize=lstream->dsize;
	  
	  if (!lives_stream_in_chunks(lstream,lstream->dsize,fbuffer,lstream->dsize*8)) {
	    do_rmem_max_error(lstream->dsize*8);
	    mainw->cancelled=CANCEL_ERROR;
	  }
	  if (mainw->cancelled) {
	    weed_free(pixel_data);
	    g_free(fbuffer);
	    return;
	  }
	  w_memcpy((guchar *)pixel_data[0]+framedataread,fbuffer,target_size);
	  dsize-=target_size;
	  fbufoffs+=target_size;
	  
	  target_size=(lstream->hsize*lstream->vsize)>>2;
	  if (target_size>dsize) target_size=dsize;
	  
	  if (target_size>0) w_memcpy((guchar *)pixel_data[1],fbuffer+fbufoffs,target_size);
	  
	  dsize-=target_size;
	  fbufoffs+=target_size;
	  
	  target_size=(lstream->hsize*lstream->vsize)>>2;
	  if (target_size>dsize) target_size=dsize;
	  
	  if (target_size>0) w_memcpy((guchar *)pixel_data[2],fbuffer+fbufoffs,target_size);
	  
	  g_free(fbuffer);
	}
#endif
      }
#ifdef USE_STRMBUF
      if (framedataread<(lstream->hsize*lstream->vsize*5)>>2) {
	target_size=((lstream->hsize*lstream->vsize*5)>>2)-framedataread;
	if (target_size>lstream->dsize) target_size=lstream->dsize;
	lives_stream_in_chunks(lstream,target_size,(guchar *)pixel_data[1]+framedataread-lstream->hsize*lstream->vsize,0);
	lstream->dsize-=target_size;
	framedataread+=target_size;
#else
	else if (framedataread<(lstream->hsize*lstream->vsize*5)>>2) {
	  target_size=((lstream->hsize*lstream->vsize*5)>>2)-framedataread;
	  if (target_size>=lstream->dsize) {
	    lives_stream_in_chunks(lstream,lstream->dsize,pixel_data[1]+framedataread-lstream->hsize*lstream->vsize,0);
	    if (mainw->cancelled) {
	      weed_free(pixel_data);
	      return;
	    }
	  }
	  else {
	    // this packet contains data for multiple planes
	    guchar *fbuffer=g_malloc(lstream->dsize);
	    size_t fbufoffs=0;
	    size_t dsize=lstream->dsize;
	    
	    lives_stream_in_chunks(lstream,lstream->dsize,fbuffer,0);
	    if (mainw->cancelled) {
	      weed_free(pixel_data);
	      g_free(fbuffer);
	      return;
	    }
	    w_memcpy((guchar *)pixel_data[1]+framedataread-lstream->hsize*lstream->vsize,fbuffer,target_size);
	    
	    dsize-=target_size;
	    fbufoffs+=target_size;
	    
	    target_size=(lstream->hsize*lstream->vsize)>>2;
	    if (target_size>dsize) target_size=dsize;
	    
	    if (target_size>0) w_memcpy((guchar *)pixel_data[2],fbuffer+fbufoffs,target_size);
	    
	    g_free(fbuffer);
	  }
#endif
	}
#ifdef USE_STRMBUF
	if (framedataread<(lstream->hsize*lstream->vsize*6)>>2) {
	  target_size=((lstream->hsize*lstream->vsize)>>2)-framedataread+((lstream->hsize*lstream->vsize*5)>>2);
	  if (target_size>lstream->dsize) target_size=lstream->dsize;
	  lives_stream_in_chunks(lstream,target_size,(guchar *)pixel_data[2]+framedataread-((lstream->hsize*lstream->vsize*5)>>2),0);
	  lstream->dsize-=target_size;
	  framedataread+=target_size;
	}
#else
	else {
	  target_size=((lstream->hsize*lstream->vsize*3)>>1)-framedataread;
	  if (target_size>=lstream->dsize) target_size=lstream->dsize;
	  lives_stream_in_chunks(lstream,target_size,pixel_data[2]+framedataread-((lstream->hsize*lstream->vsize*5)>>2),0);
	  if (mainw->cancelled) {
	    weed_free(pixel_data);
	    return;
	  }
	}
	framedataread+=lstream->dsize;
#endif
	weed_free(pixel_data);
	if (framedataread>=(lstream->hsize*lstream->vsize*3)>>1) {
	  return;
	}
	myflags|=LIVES_VSTREAM_FLAGS_IS_CONTINUATION;
	break;
      }
    }
  }





//////////////////////////////

// gui bits

void on_send_lives2lives_activate (GtkMenuItem *menuitem, gpointer user_data) {
  _vppaw *vppa;
  gchar *orig_name=g_strdup(mainw->none_string);
  gchar *tmp;
  int resp;

  if (mainw->vpp!=NULL) {
    g_free(orig_name);
    orig_name=g_strdup(mainw->vpp->name);
  }

  if (mainw->vpp==NULL||strcmp(mainw->vpp->name,"lives2lives_stream")) {
    g_snprintf(future_prefs->vpp_name,64,"lives2lives_stream");
  }
  vppa=on_vpp_advanced_clicked(NULL,NULL);
  resp=gtk_dialog_run(GTK_DIALOG(vppa->dialog));

  if (resp==GTK_RESPONSE_CANCEL) {
    g_free(orig_name);
    return;
  }

  set_vpp(FALSE);
  
  if (strcmp(orig_name,"lives2lives_stream")) {
    do_error_dialog((tmp=g_strdup_printf(_("\nLiVES will stream whenever it is in full screen/separate window mode.\nTo reset this behaviour, go to Tools/Preferences/Playback,\nand set the playback plugin back to %s\n"),orig_name)));
    g_free(tmp);
  }
  g_free(orig_name);
}





void on_open_lives2lives_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *host=NULL;
  gint port=0;

  lives_pandh_w *pandh=create_pandh_dialog(0);
  gint response=gtk_dialog_run (GTK_DIALOG (pandh->dialog));

  if (response==GTK_RESPONSE_OK) {
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pandh->rb_anyhost))) {
      host=g_strdup_printf("%s.%s.%s.%s",gtk_entry_get_text(GTK_ENTRY(pandh->entry1)),gtk_entry_get_text(GTK_ENTRY(pandh->entry2)),gtk_entry_get_text(GTK_ENTRY(pandh->entry3)),gtk_entry_get_text(GTK_ENTRY(pandh->entry4)));
    }
    else host=g_strdup("INADDR_ANY");
    port=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(pandh->port_spin));
  }

  gtk_widget_destroy (pandh->dialog);
  g_free(pandh);
  
  while (g_main_context_iteration(NULL,FALSE));

  if (host!=NULL) {
    // start receiving
    lives2lives_read_stream(host,port);
    g_free(host);
  }
}





static void pandhw_anyhost_toggled(GtkToggleButton *tbut, gpointer user_data) {
  lives_pandh_w *pandhw=(lives_pandh_w *)user_data;

  if (gtk_toggle_button_get_active(tbut)) {
    gtk_widget_set_sensitive(pandhw->entry1,FALSE);
    gtk_widget_set_sensitive(pandhw->entry2,FALSE);
    gtk_widget_set_sensitive(pandhw->entry3,FALSE);
    gtk_widget_set_sensitive(pandhw->entry4,FALSE);
  }
  else {
    gtk_widget_set_sensitive(pandhw->entry1,TRUE);
    gtk_widget_set_sensitive(pandhw->entry2,TRUE);
    gtk_widget_set_sensitive(pandhw->entry3,TRUE);
    gtk_widget_set_sensitive(pandhw->entry4,TRUE);
  }
}



lives_pandh_w* create_pandh_dialog (gint type) {
  // type = 0 lives2lives stream input

  GtkWidget *dialog_vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *radiobutton;
  GtkWidget *cancelbutton;
  GtkWidget *okbutton;
  GtkObject *spinbutton_adj;
  GtkWidget *hseparator;
  GtkWidget *eventbox;

  GSList *radiobutton_group = NULL;

  lives_pandh_w *pandhw=(lives_pandh_w *)(g_malloc(sizeof(lives_pandh_w)));

  pandhw->dialog = gtk_dialog_new ();

  gtk_window_set_position (GTK_WINDOW (pandhw->dialog), GTK_WIN_POS_CENTER_ALWAYS);
  if (prefs->show_gui) {
    gtk_window_set_transient_for(GTK_WINDOW(pandhw->dialog),GTK_WINDOW(mainw->LiVES));
  }
  gtk_window_set_modal (GTK_WINDOW (pandhw->dialog), TRUE);

  if (palette->style&STYLE_1) {
    gtk_dialog_set_has_separator(GTK_DIALOG(pandhw->dialog),FALSE);
    gtk_widget_modify_bg (pandhw->dialog, GTK_STATE_NORMAL, &palette->normal_back);
  }

  gtk_window_set_default_size (GTK_WINDOW (pandhw->dialog), 300, 200);

  gtk_container_set_border_width (GTK_CONTAINER (pandhw->dialog), 10);

  if (type==0)
    gtk_window_set_title (GTK_WINDOW (pandhw->dialog), _("LiVES: - Receive LiVES stream"));

  dialog_vbox = GTK_DIALOG (pandhw->dialog)->vbox;

  label=gtk_label_new(_("You can receive streams from another copy of LiVES."));
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 10);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  label=gtk_label_new(_("In the source copy of LiVES, you must select Advanced/Send stream to LiVES\nor select the lives2lives_stream playback plugin in Preferences."));
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 10);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hseparator = gtk_hseparator_new ();
  gtk_widget_show (hseparator);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hseparator, TRUE, TRUE, 0);

  label=gtk_label_new(_("Select the host to receive the stream from (or allow any host to stream)."));
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 10);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 10);

  pandhw->rb_anyhost = gtk_radio_button_new (NULL);
  gtk_radio_button_set_group (GTK_RADIO_BUTTON (pandhw->rb_anyhost), radiobutton_group);
  radiobutton_group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (pandhw->rb_anyhost));

  gtk_box_pack_start (GTK_BOX (hbox), pandhw->rb_anyhost, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Accept LiVES streams from _any host"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),pandhw->rb_anyhost);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Accept incoming LiVES streams from any connected host."));
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    pandhw->rb_anyhost);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  g_signal_connect_after (GTK_OBJECT (pandhw->rb_anyhost), "toggled",
			  G_CALLBACK (pandhw_anyhost_toggled),
			  (gpointer)pandhw);

  radiobutton=gtk_radio_button_new(radiobutton_group);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 10);

  gtk_box_pack_start (GTK_BOX (hbox), radiobutton, FALSE, FALSE, 10);

  label=gtk_label_new_with_mnemonic (_ ("Accept LiVES streams only from the _specified host:"));
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),radiobutton);

  eventbox=gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(eventbox),label);
  gtk_widget_set_tooltip_text( eventbox, _("Accept LiVES streams from the specified host only."));
  g_signal_connect (GTK_OBJECT (eventbox), "button_press_event",
		    G_CALLBACK (label_act_toggle),
		    radiobutton);

  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_fg(eventbox, GTK_STATE_NORMAL, &palette->normal_fore);
    gtk_widget_modify_bg (eventbox, GTK_STATE_NORMAL, &palette->normal_back);
  }
  gtk_box_pack_start (GTK_BOX (hbox), eventbox, FALSE, FALSE, 10);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(hbox, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  ////////////////////////////////////


  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 10);

  pandhw->entry1 = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY (pandhw->entry1),3);
  gtk_entry_set_width_chars (GTK_ENTRY (pandhw->entry1),3);
  gtk_box_pack_start (GTK_BOX (hbox), pandhw->entry1, FALSE, FALSE, 10);
  gtk_entry_set_text(GTK_ENTRY(pandhw->entry1),"127");

  label=gtk_label_new(".");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  pandhw->entry2 = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY (pandhw->entry2),3);
  gtk_entry_set_width_chars (GTK_ENTRY (pandhw->entry2),3);
  gtk_box_pack_start (GTK_BOX (hbox), pandhw->entry2, FALSE, FALSE, 10);
  gtk_entry_set_text(GTK_ENTRY(pandhw->entry2),"0");

  label=gtk_label_new(".");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  pandhw->entry3 = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY (pandhw->entry3),3);
  gtk_entry_set_width_chars (GTK_ENTRY (pandhw->entry3),3);
  gtk_box_pack_start (GTK_BOX (hbox), pandhw->entry3, FALSE, FALSE, 10);
  gtk_entry_set_text(GTK_ENTRY(pandhw->entry3),"0");

  label=gtk_label_new(".");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  pandhw->entry4 = gtk_entry_new ();
  gtk_entry_set_max_length(GTK_ENTRY (pandhw->entry4),3);
  gtk_entry_set_width_chars (GTK_ENTRY (pandhw->entry4),3);
  gtk_box_pack_start (GTK_BOX (hbox), pandhw->entry4, FALSE, FALSE, 10);
  gtk_entry_set_text(GTK_ENTRY(pandhw->entry4),"1");

  gtk_widget_set_sensitive(pandhw->entry1,FALSE);
  gtk_widget_set_sensitive(pandhw->entry2,FALSE);
  gtk_widget_set_sensitive(pandhw->entry3,FALSE);
  gtk_widget_set_sensitive(pandhw->entry4,FALSE);

  label=gtk_label_new(_("Enter the port number to listen for LiVES streams on:"));
  gtk_box_pack_start (GTK_BOX (dialog_vbox), label, TRUE, TRUE, 10);
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), hbox, TRUE, TRUE, 10);

  label=gtk_label_new(_("Port"));
  if (palette->style&STYLE_1) {
    gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
  }
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  spinbutton_adj = gtk_adjustment_new (8888, 1., 65535., 1., 1., 0.);
  pandhw->port_spin = gtk_spin_button_new (GTK_ADJUSTMENT (spinbutton_adj), 1., 0);
  gtk_entry_set_activates_default (GTK_ENTRY ((GtkEntry *)&(GTK_SPIN_BUTTON (pandhw->port_spin)->entry)), TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label),pandhw->port_spin);
  gtk_box_pack_start (GTK_BOX (hbox), pandhw->port_spin, FALSE, FALSE, 10);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (pandhw->port_spin),GTK_UPDATE_IF_VALID);


  cancelbutton = gtk_button_new_from_stock ("gtk-cancel");
  gtk_dialog_add_action_widget (GTK_DIALOG (pandhw->dialog), cancelbutton, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (cancelbutton, GTK_CAN_DEFAULT);

  okbutton = gtk_button_new_from_stock ("gtk-ok");
  gtk_dialog_add_action_widget (GTK_DIALOG (pandhw->dialog), okbutton, GTK_RESPONSE_OK);
  GTK_WIDGET_SET_FLAGS (okbutton, GTK_CAN_DEFAULT);
  gtk_widget_grab_default (okbutton);

  gtk_widget_show_all (pandhw->dialog);

  return pandhw;
}
