// LiVES - ogg/theora/dirac/vorbis decoder plugin
// (c) G. Finch 2008 - 2010 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


// start decoding at the page whose granulepos converts to the highest frame number prior to the one you're looking for


// TODO - split this into demuxer and decoders

// ignore this, just some notes ----------
// decoders should have the functions:
// - recognise :: demuxer passes in the first packet of a stream, and a decoder claims it
//                decoder tells how many more bos packets it needs (detect_stream)
// - pre_init :: demuxer passes in bos packets until the decoder has parsed them all (setup tracks)
// - init :: decoder initialises its internal data (attach_stream)
// - set_data :: decoder completes cdata

// - clear  :: decoder frees stuff (detach_stream)


// granulepos to pos :: demuxer passes in something like a granulepos or a byte offset, decoder returns kframe,frame or
//                      sample offset   [is this possible since it requires knowledge of demuxer and decoder ?]
//                      (granulepos_to_time)


// granulepos_2_frame and frame_2 _granulepos are problematic for splitting like this

// - decode packet (and return if we got a frame out) (ogg_*_read)

// - return same frame

// finalise (clip_data_free)
///----------------------------------------------------------


#include "decplugin.h"

///////////////////////////////////////////////////////
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#include <ogg/ogg.h>

#ifdef HAVE_THEORA
#include <theora/theora.h>
#endif

#ifdef HAVE_DIRAC
#include <schroedinger/schro.h>
#endif

#include <pthread.h>

#include "ogg_decoder.h"



static boolean ogg_data_process(lives_clip_data_t *cdata, void *yuvbuffer, boolean cont);


static const char *plugin_version="LiVES ogg decoder version 1.2";

static index_container_t **indices;
static int nidxc;
static pthread_mutex_t indices_mutex;

/////////////////////////////////////////
// schroed stuff


#ifdef HAVE_DIRAC
static void SchroFrameFree(SchroFrame *sframe, void *priv) {
  register int i;
  for (i=0; i<3; i++) free(sframe->components[i].data);
}


static void SchroBufferFree(SchroBuffer *sbuf, void *priv) {
  free(priv);
}


#endif


////////////////////////////////////////////

// index_entries



static void index_entries_free(index_entry *idx) {
  index_entry *idx_next;

  while (idx!=NULL) {
    idx_next=idx->next;
    free(idx);
    idx=idx_next;
  }
}



static index_entry *index_entry_new(void) {
  index_entry *ie=(index_entry *)malloc(sizeof(index_entry));
  ie->next=ie->prev=NULL;
  ie->pagepos_end=-1;
  return ie;
}


#ifdef HAVE_DIRAC

static index_entry *index_entry_delete(index_entry *idx) {
  // unlink and free idx. If idx is head of list, return new head.

  index_entry *xidx=idx;

  if (idx->prev!=NULL) idx->prev->next=idx->next;
  else xidx=idx->next;

  if (idx->next!=NULL) idx->next->prev=idx->prev;
  free(idx);

  return xidx;
}


static index_entry *find_pagepos_in_index(index_entry *idx, int64_t pagepos) {
  while (idx!=NULL) {
    if (idx->pagepos<=pagepos&&idx->pagepos_end>=pagepos) return idx;
    idx=idx->next;
  }
  return NULL;
}

#endif


static index_entry *theora_index_entry_add(lives_clip_data_t *cdata, int64_t granule, int64_t pagepos) {
  // add or update entry for keyframe and return it
  index_entry *idx,*oidx,*last_idx=NULL;
  int64_t gpos,frame,kframe,tframe,tkframe;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  if (priv->vstream==NULL) return NULL;

  tkframe = granule >> priv->vstream->stpriv->keyframe_granule_shift;
  tframe = tkframe + granule-(tkframe<<priv->vstream->stpriv->keyframe_granule_shift);

  if (tkframe<1) return NULL;

  oidx=idx=priv->idxc->idx;

  if (idx==NULL) {
    index_entry *ie=index_entry_new();
    ie->value=granule;
    ie->pagepos=pagepos;
    priv->idxc->idx=ie;
    return ie;
  }

  while (idx!=NULL) {
    gpos=idx->value;

    kframe = gpos >> priv->vstream->stpriv->keyframe_granule_shift;
    if (kframe>tframe) break;

    if (kframe==tkframe) {
      // entry exists, update it if applicable, and return it in found
      frame = kframe + gpos-(kframe<<priv->vstream->stpriv->keyframe_granule_shift);
      if (frame<tframe) {
        idx->value=granule;
        idx->pagepos=pagepos;
      }
      return idx;
    }

    last_idx=idx;
    idx=idx->next;
  }

  // insert after last_idx

  idx=index_entry_new();

  if (last_idx!=NULL) {
    idx->next=last_idx->next;
    last_idx->next=idx;
    idx->prev=last_idx;
  } else {
    idx->next=oidx;
    oidx=idx;
  }

  if (idx->next!=NULL) {
    idx->next->prev=idx;
  }

  idx->value=granule;
  idx->pagepos=pagepos;

  return idx;
}

#ifdef HAVE_DIRAC

static index_entry *dirac_index_entry_add_before(index_entry *idx, int64_t pagepos) {
  index_entry *new_idx=index_entry_new();

  new_idx->value=-1;
  new_idx->pagepos=pagepos;

  if (idx!=NULL) {
    new_idx->next=idx;
    new_idx->prev=idx->prev;
    if (idx->prev!=NULL) idx->prev->next=new_idx;
    idx->prev=new_idx;
  }
  return new_idx;
}



static index_entry *dirac_index_entry_add(lives_clip_data_t *cdata, int64_t pagepos, int64_t pagepos_end,
    int64_t frame) {
  // add a new entry in order, and return a pointer to it

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  index_entry *new_idx=priv->idxc->idx,*last_idx=NULL;

  //printf("ADDING IDX for frame %ld region %ld to %ld\n",frame,pagepos,pagepos_end);

  //if (pagepos_end<pagepos) return NULL; // TODO ***

  while (new_idx!=NULL) {
    if (new_idx->pagepos>pagepos) {
      new_idx=dirac_index_entry_add_before(new_idx,pagepos);
      new_idx->value=frame;
      new_idx->pagepos_end=pagepos_end;

      return new_idx;
    }
    last_idx=new_idx;
    new_idx=new_idx->next;
  }

  // add as last enry

  new_idx=index_entry_new();
  new_idx->value=frame;
  new_idx->pagepos=pagepos;
  new_idx->pagepos_end=pagepos_end;

  if (last_idx!=NULL) {
    new_idx->prev=last_idx;
    last_idx->next=new_idx;
  }


  return new_idx;

}

#endif



static index_entry *get_bounds_for(lives_clip_data_t *cdata, int64_t tframe, int64_t *ppos_lower, int64_t *ppos_upper) {
  // find upper and lower pagepos for frame; if we find an exact match, we return it
  int64_t kframe,frame,gpos;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  index_entry *idx=priv->idxc->idx;

  *ppos_lower=*ppos_upper=-1;

  while (idx!=NULL) {

    if (idx->pagepos<0) {
      // kframe was found to be invalid
      idx=idx->next;
      continue;
    }

    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
      gpos=idx->value;
      kframe = gpos >> priv->vstream->stpriv->keyframe_granule_shift;
      frame = kframe + gpos-(kframe<<priv->vstream->stpriv->keyframe_granule_shift);
    } else {
      kframe=frame=idx->value;
    }

    //fprintf(stderr,"check %lld against %lld\n",tframe,kframe);

    if (kframe>tframe) {
      *ppos_upper=idx->pagepos;
      return NULL;
    }


    //fprintf(stderr,"2check against %lld\n",frame);
    if (frame<tframe) {
      *ppos_lower=idx->pagepos;
      idx=idx->next;
      continue;
    }
    //fprintf(stderr,"gotit 1\n");
    return idx;
  }
  //fprintf(stderr,"not 1\n");
  return NULL;
}




/////////////////////////////////////////////////////

#ifdef HAVE_THEORA
static uint8_t *ptr_2_op(uint8_t *ptr, ogg_packet *op) {
  memcpy(op, ptr, sizeof(*op));
  ptr += sizeof(*op);
  op->packet = ptr;
  ptr += op->bytes;
  return ptr;
}
#endif


static int64_t get_page(lives_clip_data_t *cdata, int64_t inpos) {
  uint8_t header[PAGE_HEADER_BYTES+255];
  int nsegs, i;
  int64_t result, gpos;
  int page_size;
  char *buf;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  if (opriv->page_valid) {
    fprintf(stderr,"page valid !\n");
    return 0;
  }

  lseek64(opriv->fd, inpos, SEEK_SET);

  if (read(opriv->fd, header, PAGE_HEADER_BYTES) < PAGE_HEADER_BYTES) {
    lseek64(opriv->fd, inpos, SEEK_SET);
    return 0;
  }

  nsegs = header[PAGE_HEADER_BYTES-1];

  if (read(opriv->fd, header+PAGE_HEADER_BYTES, nsegs) < nsegs) {
    lseek64(opriv->fd, inpos, SEEK_SET);
    return 0;
  }

  page_size = PAGE_HEADER_BYTES + nsegs;

  for (i=0; i<nsegs; i++) page_size += header[PAGE_HEADER_BYTES+i];

  ogg_sync_reset(&opriv->oy);

  buf = ogg_sync_buffer(&(opriv->oy), page_size);

  memcpy(buf,header,PAGE_HEADER_BYTES+nsegs);

  result = read(opriv->fd, (uint8_t *)buf+PAGE_HEADER_BYTES+nsegs, page_size-PAGE_HEADER_BYTES-nsegs);

  ogg_sync_wrote(&(opriv->oy), result+PAGE_HEADER_BYTES+nsegs);

  if (ogg_sync_pageout(&(opriv->oy), &(opriv->current_page)) != 1) {
    //fprintf(stderr, "Got no packet %lld %d %s\n",result,page_size,buf);
    return 0;
  }

  //fprintf(stderr, "Got packet %lld %d %s\n",result,page_size,buf);

  if (priv->vstream!=NULL) {
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
      if (ogg_page_serialno(&(opriv->current_page))==priv->vstream->stream_id) {
        gpos=ogg_page_granulepos(&(opriv->current_page));
        pthread_mutex_lock(&priv->idxc->mutex);
        theora_index_entry_add(cdata, gpos, inpos);
        pthread_mutex_unlock(&priv->idxc->mutex);
      }
    }
  }

  opriv->page_valid = 1;
  return result+PAGE_HEADER_BYTES+nsegs;
}



static uint32_t detect_stream(ogg_packet *op) {
  // TODO - do this in the decoder plugins (not in demuxer)

  //fprintf(stderr,"detecting stream\n");
  if ((op->bytes > 7) &&
      (op->packet[0] == 0x01) &&
      !strncmp((char *)(op->packet+1), "vorbis", 6)) {
    //fprintf(stderr,"vorbis found\n");
    return FOURCC_VORBIS;
  } else if ((op->bytes > 7) &&
             (op->packet[0] == 0x80) &&
             !strncmp((char *)(op->packet+1), "theora", 6)) {
    //fprintf(stderr,"theora found\n");
    return FOURCC_THEORA;
  } else if ((op->bytes > 5) &&
             (op->packet[4] == 0x00) &&
             !strncmp((char *)(op->packet), "BBCD", 4)) {
    //fprintf(stderr,"dirac found\n");
    return FOURCC_DIRAC;
  } else if ((op->bytes > 8) &&
             (op->packet[8] == 0x00) &&
             !strncmp((char *)(op->packet), "KW-DIRAC", 8)) {
    //fprintf(stderr,"dirac found\n");
    return FOURCC_DIRAC;
  }

  return 0;
}

#define PTR_2_32BE(p) \
  ((*(p) << 24) | \
   (*(p+1) << 16) | \
   (*(p+2) << 8) | \
   *(p+3))

#define PTR_2_32LE(p) \
  ((*(p+3) << 24) | \
   (*(p+2) << 16) | \
   (*(p+1) << 8) | \
   *(p))


static lives_in_stream *lives_in_stream_new(int type) {
  lives_in_stream *lstream=(lives_in_stream *)malloc(sizeof(lives_in_stream));
  lstream->type=type;
  lstream->ext_data=NULL;
  lstream->ext_size=0;
  return lstream;
}


static void append_extradata(lives_in_stream *s, ogg_packet *op) {
  s->ext_data = realloc(s->ext_data, s->ext_size + sizeof(*op) + op->bytes);
  memcpy(s->ext_data + s->ext_size, op, sizeof(*op));
  memcpy(s->ext_data + s->ext_size + sizeof(*op), op->packet, op->bytes);
  s->ext_size += (sizeof(*op) + op->bytes);
}


static inline lives_in_stream *stream_from_sno(lives_clip_data_t *cdata, int sno) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  if (priv->astream!=NULL&&sno==priv->astream->stream_id) return priv->astream;
  if (priv->vstream!=NULL&&sno==priv->vstream->stream_id) return priv->vstream;
  return NULL;
}






static uint32_t dirac_uint(bs_t *p_bs) {
  uint32_t u_count = 0, u_value = 0;

  while (!bs_eof(p_bs) && !bs_read(p_bs, 1)) {
    u_count++;
    u_value <<= 1;
    u_value |= bs_read(p_bs, 1);
  }

  return (1<<u_count) - 1 + u_value;
}

static int dirac_bool(bs_t *p_bs) {
  return bs_read(p_bs, 1);
}



static index_container_t *idxc_for(lives_clip_data_t *cdata) {
  // check all idxc for string match with URI
  index_container_t *idxc;
  register int i;

  pthread_mutex_lock(&indices_mutex);

  for (i=0; i<nidxc; i++) {
    if (indices[i]->clients[0]->current_clip==cdata->current_clip&&
        !strcmp(indices[i]->clients[0]->URI,cdata->URI)) {
      idxc=indices[i];
      // append cdata to clients
      idxc->clients=(lives_clip_data_t **)realloc(idxc->clients,(idxc->nclients+1)*sizeof(lives_clip_data_t *));
      idxc->clients[idxc->nclients]=cdata;
      idxc->nclients++;
      //
      pthread_mutex_unlock(&indices_mutex);
      return idxc;
    }
  }

  indices=(index_container_t **)realloc(indices,(nidxc+1)*sizeof(index_container_t *));

  // match not found, create a new index container
  idxc=(index_container_t *)malloc(sizeof(index_container_t));

  idxc->idx=NULL;

  idxc->nclients=1;
  idxc->clients=(lives_clip_data_t **)malloc(sizeof(lives_clip_data_t *));
  idxc->clients[0]=cdata;
  pthread_mutex_init(&idxc->mutex,NULL);

  indices[nidxc]=idxc;
  pthread_mutex_unlock(&indices_mutex);

  nidxc++;

  return idxc;
}


static void idxc_release(lives_clip_data_t *cdata) {
  lives_ogg_priv_t *priv=cdata->priv;
  index_container_t *idxc=priv->idxc;
  register int i,j;

  if (idxc==NULL) return;

  pthread_mutex_lock(&indices_mutex);

  if (idxc->nclients==1) {
    // remove this index
    index_entries_free(idxc->idx);
    free(idxc->clients);
    for (i=0; i<nidxc; i++) {
      if (indices[i]==idxc) {
        nidxc--;
        for (j=i; j<nidxc; j++) {
          indices[j]=indices[j+1];
        }
        free(idxc);
        if (nidxc==0) {
          free(indices);
          indices=NULL;
        } else indices=(index_container_t **)realloc(indices,nidxc*sizeof(index_container_t *));
        break;
      }
    }
  } else {
    // reduce client count by 1
    for (i=0; i<idxc->nclients; i++) {
      if (idxc->clients[i]==cdata) {
        // remove this entry
        idxc->nclients--;
        for (j=i; j<idxc->nclients; j++) {
          idxc->clients[j]=idxc->clients[j+1];
        }
        idxc->clients=(lives_clip_data_t **)realloc(idxc->clients,idxc->nclients*sizeof(lives_clip_data_t *));
        break;
      }
    }
  }

  pthread_mutex_unlock(&indices_mutex);

}


static void idxc_release_all(void) {
  register int i;

  for (i=0; i<nidxc; i++) {
    index_entries_free(indices[i]->idx);
    free(indices[i]->clients);
    free(indices[i]);
  }
  nidxc=0;

}




static int setup_tracks(lives_clip_data_t *cdata) {
  // pull first audio and video streams

  // some constants for dirac (from vlc)
  static const struct {
    uint32_t u_n /* numerator */, u_d /* denominator */;
  } p_dirac_frate_tbl[] = { /* table 10.3 */
    {1,1}, /* this first value is never used */
    {24000,1001}, {24,1}, {25,1}, {30000,1001}, {30,1},
    {50,1}, {60000,1001}, {60,1}, {15000,1001}, {25,2},
  };
  static const size_t u_dirac_frate_tbl = sizeof(p_dirac_frate_tbl)/sizeof(*p_dirac_frate_tbl);

  static const uint32_t pu_dirac_vidfmt_frate[] = { /* table C.1 */
    1, 9, 10, 9, 10, 9, 10, 4, 3, 7, 6, 4, 3, 7, 6, 2, 2, 7, 6, 7, 6,
  };
  static const size_t u_dirac_vidfmt_frate = sizeof(pu_dirac_vidfmt_frate)/sizeof(*pu_dirac_vidfmt_frate);



  ////////////////////////////////


  int done;
  stream_priv_t *ogg_stream;
  int serialno;
  int header_bytes = 0;
  int64_t input_pos;

  uint8_t imajor,iminor;

#ifdef HAVE_THEORA
  uint8_t isubminor;
#endif

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  uint32_t u_video_format,u_n,u_d;

  bs_t bs;


  opriv->page_valid=0;

  lseek64(opriv->fd, 0, SEEK_SET);
  priv->input_position=0;

  /* Get the first page of each stream */
  while (1) {

    if (!(input_pos=get_page(cdata,priv->input_position))) {
      //fprintf(stderr, "EOF1 while setting up track\n");
      return 0;
    }

    priv->input_position+=input_pos;

    if (!ogg_page_bos(&(opriv->current_page))) {
      opriv->page_valid = 1;
      break;
    }

    /* Setup stream */
    serialno = ogg_page_serialno(&(opriv->current_page));
    ogg_stream = calloc(1, sizeof(*ogg_stream));
    ogg_stream->last_granulepos = -1;

    ogg_stream_init(&ogg_stream->os, serialno);
    ogg_stream_pagein(&ogg_stream->os, &(opriv->current_page));
    opriv->page_valid = 0;
    header_bytes += opriv->current_page.header_len + opriv->current_page.body_len;

    if (ogg_stream_packetout(&ogg_stream->os, &opriv->op) != 1) {
      fprintf(stderr, "EOF3 while setting up track\n");
      ogg_stream_clear(&ogg_stream->os);
      free(ogg_stream);
      return 0;
    }


    // TODO - do stream detection in decoder plugins

    ogg_stream->fourcc_priv = detect_stream(&opriv->op);

    switch (ogg_stream->fourcc_priv) {
    case FOURCC_VORBIS:
      if (priv->astream!=NULL) {
        fprintf(stderr,"got extra audio stream\n");
        ogg_stream_clear(&ogg_stream->os);
        free(ogg_stream);
        break;
      }
      priv->astream = lives_in_stream_new(LIVES_STREAM_AUDIO);
      priv->astream->fourcc = FOURCC_VORBIS;
      priv->astream->stpriv   = ogg_stream;
      priv->astream->stream_id = serialno;

      ogg_stream->header_packets_needed = 3;
      append_extradata(priv->astream, &opriv->op);
      ogg_stream->header_packets_read = 1;

      /* Get samplerate */
      priv->astream->samplerate=PTR_2_32LE(opriv->op.packet + 12);
      //fprintf(stderr,"rate is %d\n",priv->astream->samplerate);

      /* Read remaining header packets from this page */
      while (ogg_stream_packetout(&ogg_stream->os, &opriv->op) == 1) {
        append_extradata(priv->astream, &opriv->op);
        ogg_stream->header_packets_read++;
        if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed)  break;
      }
      break;
#ifdef HAVE_THEORA
    case FOURCC_THEORA:
      if (priv->vstream!=NULL) {
        fprintf(stderr,"got extra video stream\n");
        ogg_stream_clear(&ogg_stream->os);
        free(ogg_stream);
        break;
      }
      priv->vstream = lives_in_stream_new(LIVES_STREAM_VIDEO);
      priv->vstream->fourcc = FOURCC_THEORA;
      priv->vstream->stpriv   = ogg_stream;
      priv->vstream->stream_id = serialno;

      ogg_stream->header_packets_needed = 3;
      append_extradata(priv->vstream, &opriv->op);
      ogg_stream->header_packets_read = 1;

      priv->vstream->data_start=0;

      /* get version */
      imajor=((uint8_t *)(priv->vstream->ext_data))[55];
      iminor=((uint8_t *)(priv->vstream->ext_data))[56];
      isubminor=((uint8_t *)(priv->vstream->ext_data))[57];

      priv->vstream->version=imajor*1000000+iminor*1000+isubminor;

      // TODO - get frame width, height, picture width, height, and x and y offsets

      cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION;


      /* Get fps and keyframe shift */
      priv->vstream->fps_num = PTR_2_32BE(opriv->op.packet+22);
      priv->vstream->fps_denom = PTR_2_32BE(opriv->op.packet+26);

      //fprintf(stderr,"fps is %d / %d\n",priv->vstream->fps_num,priv->vstream->fps_denom);

      ogg_stream->keyframe_granule_shift = (char)((opriv->op.packet[40] & 0x03) << 3);
      ogg_stream->keyframe_granule_shift |= (opriv->op.packet[41] & 0xe0) >> 5;

      /* Read remaining header packets from this page */
      while (ogg_stream_packetout(&ogg_stream->os, &opriv->op) == 1) {
        append_extradata(priv->vstream, &opriv->op);
        ogg_stream->header_packets_read++;
        if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed) break;
      }

      break;
#endif
    case FOURCC_DIRAC:
      if (priv->vstream!=NULL) {
        fprintf(stderr,"got extra video stream\n");
        ogg_stream_clear(&ogg_stream->os);
        free(ogg_stream);
        break;
      }

      priv->vstream = lives_in_stream_new(LIVES_STREAM_VIDEO);
      priv->vstream->fourcc = FOURCC_DIRAC;
      priv->vstream->stpriv   = ogg_stream;
      priv->vstream->stream_id = serialno;

      priv->vstream->data_start=priv->input_position-input_pos;

      ogg_stream->header_packets_needed = 0;
      ogg_stream->header_packets_read = 1;

      ogg_stream->keyframe_granule_shift = 22; /* not 32 */

      /* read in useful bits from sequence header */
      bs_init(&bs, &opriv->op.packet, opriv->op.bytes);

      /* get version */
      bs_skip(&bs, 13*8);  /* parse_info_header */

      imajor=dirac_uint(&bs);   /* major_version */
      iminor=dirac_uint(&bs);   /* minor_version */
      //iprof=dirac_uint( &bs ); /* profile */
      //ilevel=dirac_uint( &bs ); /* level */

      priv->vstream->version=imajor*1000000+iminor*1000;

      //printf("dirac version %d\n",priv->vstream->version);

      u_video_format = dirac_uint(&bs);   /* index */
      if (u_video_format >= u_dirac_vidfmt_frate) {
        /* don't know how to parse this ogg dirac stream */
        ogg_stream_clear(&ogg_stream->os);
        free(ogg_stream);
        free(priv->vstream);
        priv->vstream=NULL;
        break;
      }

      if (dirac_bool(&bs)) {
        cdata->frame_width=cdata->width=dirac_uint(&bs);   /* frame_width */
        cdata->frame_height=cdata->width=dirac_uint(&bs);   /* frame_height */
      }
      // else...????

      if (dirac_bool(&bs)) {
        dirac_uint(&bs);   /* chroma_format */
      }

      if (dirac_bool(&bs)) {
        dirac_uint(&bs);   /* scan_format */
      }

      u_n = p_dirac_frate_tbl[pu_dirac_vidfmt_frate[u_video_format]].u_n;
      u_d = p_dirac_frate_tbl[pu_dirac_vidfmt_frate[u_video_format]].u_d;

      if (dirac_bool(&bs)) {
        uint32_t u_frame_rate_index = dirac_uint(&bs);
        if (u_frame_rate_index >= u_dirac_frate_tbl) {
          /* something is wrong with this stream */
          ogg_stream_clear(&ogg_stream->os);
          free(ogg_stream);
          free(priv->vstream);
          priv->vstream=NULL;
          break;
        }
        u_n = p_dirac_frate_tbl[u_frame_rate_index].u_n;
        u_d = p_dirac_frate_tbl[u_frame_rate_index].u_d;
        if (u_frame_rate_index == 0) {
          u_n = dirac_uint(&bs);   /* frame_rate_numerator */
          u_d = dirac_uint(&bs);   /* frame_rate_denominator */
        }
      }

      cdata->fps = (float) u_n / (float) u_d;

      cdata->seek_flag=LIVES_SEEK_FAST|LIVES_SEEK_NEEDS_CALCULATION|LIVES_SEEK_QUALITY_LOSS;

      priv->vstream->fps_num = u_n;
      priv->vstream->fps_denom = u_d;

      break;

    default:
      ogg_stream_clear(&ogg_stream->os);
      free(ogg_stream);
      break;
    }
  }

  if (priv->vstream==NULL) return 0;


  /*
   *  Now, read header pages until we are done, current_page still contains the
   *  first page which has no bos marker set
   */

  done = 0;
  while (!done) {
    lives_in_stream *stream = stream_from_sno(cdata,ogg_page_serialno(&(opriv->current_page)));
    if (stream) {
      ogg_stream = (stream_priv_t *)(stream->stpriv);
      ogg_stream_pagein(&(ogg_stream->os), &(opriv->current_page));
      opriv->page_valid = 0;
      header_bytes += opriv->current_page.header_len + opriv->current_page.body_len;

      switch (ogg_stream->fourcc_priv) {
      case FOURCC_THEORA:
      case FOURCC_VORBIS:
        /* Read remaining header packets from this page */
        while (ogg_stream_packetout(&ogg_stream->os, &opriv->op) == 1) {
          append_extradata(stream, &opriv->op);
          ogg_stream->header_packets_read++;
          /* Second packet is vorbis comment starting after 7 bytes */
          //if (ogg_stream->header_packets_read == 2) parse_vorbis_comment(stream, opriv->op.packet + 7, opriv->op.bytes - 7);
          if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed) break;
        }
        break;
      }
    } else {
      opriv->page_valid = 0;
      header_bytes += opriv->current_page.header_len + opriv->current_page.body_len;
    }

    /* Check if we are done for all streams */
    done = 1;

    //for (i = 0; i < track->num_audio_streams; i++) {
    if (priv->astream!=NULL) {
      ogg_stream = priv->astream->stpriv;
      if (ogg_stream->header_packets_read < ogg_stream->header_packets_needed) done = 0;
    }

    if (done) {
      //for(i = 0; i < track->num_video_streams; i++)
      //{
      if (priv->vstream!=NULL) {
        ogg_stream = priv->vstream->stpriv;
        if (ogg_stream->header_packets_read < ogg_stream->header_packets_needed) done = 0;
      }
    }
    //}

    /* Read the next page if we aren't done yet */

    if (!done) {
      if (!(input_pos=get_page(cdata,priv->input_position))) {
        fprintf(stderr, "EOF2 while setting up track");
        return 0;
      }
      priv->input_position+=input_pos;
    }
  }

  if (priv->vstream->data_start==0) priv->vstream->data_start=priv->input_position;

  return 1;
}


#ifdef HAVE_DIRAC
void get_dirac_cdata(lives_clip_data_t *cdata, SchroDecoder *schrodec) {
  SchroVideoFormat *sformat=schro_decoder_get_video_format(schrodec);
  cdata->frame_width=sformat->width;
  cdata->frame_height=sformat->height;

  cdata->width=sformat->clean_width;
  cdata->height=(sformat->clean_height>>1)<<1;


  if (sformat->interlaced) {
    if (sformat->top_field_first) cdata->interlace=LIVES_INTERLACE_TOP_FIRST;
    else cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;
  } else cdata->interlace=LIVES_INTERLACE_NONE;

  switch (sformat->chroma_format) {
  case SCHRO_CHROMA_420:
    cdata->palettes[0]=WEED_PALETTE_YUV420P;
    break;
  case SCHRO_CHROMA_422:
    cdata->palettes[0]=WEED_PALETTE_YUV422P;
    break;
  case SCHRO_CHROMA_444:
    cdata->palettes[0]=WEED_PALETTE_YUV444P;
    break;
  default:
    cdata->palettes[0]=WEED_PALETTE_END;
    break;
  }

  //if (sformat->colour_matrix==SCHRO_COLOUR_MATRIX_HDTV) cdata->YUV_subspace=WEED_YUV_SUBSPACE_BT709;

  cdata->offs_x = sformat->left_offset;
  cdata->offs_y = sformat->top_offset;

  cdata->par = sformat->aspect_ratio_numerator/sformat->aspect_ratio_denominator;

  if (sformat->luma_offset==0) cdata->YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
  else cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;

  //printf("vals %d %d %d %d %d %d\n",sformat->width,sformat->height,sformat->clean_width,sformat->clean_height,sformat->interlaced,cdata->palettes[0]);

  free(sformat);

}
#endif





static void seek_byte(lives_clip_data_t *cdata, int64_t pos) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  ogg_sync_reset(&(opriv->oy));
  lseek64(opriv->fd,pos,SEEK_SET);
  priv->input_position=pos;
  opriv->page_valid=0;
}


/* Get new data */

static int64_t get_data(lives_clip_data_t *cdata, size_t bytes_to_read) {
  char *buf;
  int64_t result;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  if (opriv->total_bytes > 0) {
    if (priv->input_position + bytes_to_read > opriv->total_bytes)
      bytes_to_read = opriv->total_bytes - priv->input_position;
    if (bytes_to_read <= 0)
      return 0;
  }

  ogg_sync_reset(&opriv->oy);
  buf = ogg_sync_buffer(&(opriv->oy), bytes_to_read);

  lseek64(opriv->fd, priv->input_position, SEEK_SET);
  result = read(opriv->fd, (uint8_t *)buf, bytes_to_read);

  opriv->page_valid=0;

  ogg_sync_wrote(&(opriv->oy), result);
  return result;
}


/* Find the first first page between pos1 and pos2,
   return file position, -1 is returned on failure. For theora only. */


static int64_t find_first_page(lives_clip_data_t *cdata, int64_t pos1, int64_t pos2, int64_t *kframe, int64_t *frame) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  int64_t bytes;
  long result;

  ogg_t *opriv=priv->opriv;

  int pages_checked=0,page_packets_checked;

  size_t bytes_to_read=pos2-pos1+1;

  int64_t granulepos;

  // check index to see if pos1 is in index


  // pull pages and check packets until we either reach the end or find a granulepos

  priv->input_position=pos1;

  //printf ("start %ld %ld\n",pos1,pos2);

  seek_byte(cdata,pos1);

  if (pos1 == priv->vstream->data_start) {
    /* set a dummy granulepos at data_start */
    *kframe = priv->kframe_offset;
    *frame = priv->kframe_offset;

    opriv->page_valid = 1;
    return priv->input_position;
  }

  if (bytes_to_read>BYTES_TO_READ) bytes_to_read=BYTES_TO_READ;

  while (1) {

    if (priv->input_position>=pos2) {
      // we reached the end and found no pages
      //printf("exit a\n");
      *frame=-1;
      return -1;
    }

    // read next chunk
    if (!(bytes=get_data(cdata,bytes_to_read))) {
      //printf("exit b\n");
      *frame=-1;
      return -1; // eof
    }

    bytes_to_read=BYTES_TO_READ;

    result = ogg_sync_pageseek(&opriv->oy, &opriv->current_page);

    //printf("result is %ld\n",result);

    if (result<0) {
      // found a page, sync to page start
      priv->input_position-=result;
      pos1=priv->input_position;
      continue;
    }

    if (result>0||(result==0&&opriv->oy.fill>3&&!strncmp((char *)opriv->oy.data,"OggS",4))) {
      pos1=priv->input_position;
      break;
    }

    priv->input_position+=bytes;

  };

  seek_byte(cdata,priv->input_position);
  ogg_stream_reset(&priv->vstream->stpriv->os);

  while (1) {

    //printf("here %ld and %ld\n",priv->input_position,pos2);

    if (priv->input_position>=pos2) {
      // reached the end of the search region and nothing was found
      *frame=-1;
      return priv->input_position; // we checked up to here
    }


    //printf("a: checking page\n");

    opriv->page_valid=0;

    if (!(result=get_page(cdata,priv->input_position))) {
      // EOF
      //printf("EOF ?\n");
      *frame=-1;
      return priv->input_position; // we checked up to here
    }

    // found a page
    if (priv->vstream->stream_id!=ogg_page_serialno(&(opriv->current_page))) {
      // page is not for this stream
      //printf("not for us\n");
      priv->input_position+=result;
      if (!pages_checked) pos1=priv->input_position;
      continue;
    }

    ogg_stream_pagein(&priv->vstream->stpriv->os, &(opriv->current_page));

    pages_checked++;

    page_packets_checked=0;

    if (ogg_stream_packetout(&priv->vstream->stpriv->os, &opriv->op) > 0) {
      page_packets_checked++;
    }

    if (page_packets_checked) {
      granulepos = ogg_page_granulepos(&(opriv->current_page));
      pthread_mutex_lock(&priv->idxc->mutex);
      theora_index_entry_add(cdata,granulepos,pos1);
      pthread_mutex_unlock(&priv->idxc->mutex);

      *kframe =
        granulepos >> priv->vstream->stpriv->keyframe_granule_shift;

      *frame = *kframe +
               granulepos-(*kframe<<priv->vstream->stpriv->keyframe_granule_shift);

      opriv->page_valid=1;
      return pos1;
    }

    //  -> start of next page
    priv->input_position+=result;

  }
}




/* Find the last frame for theora,
   -1 is returned on failure */

static int64_t find_last_theora_frame(lives_clip_data_t *cdata, lives_in_stream *vstream) {

  int64_t page_pos, start_pos;
  int64_t this_frame=0, last_frame = -1;
  int64_t this_kframe=0;
  int64_t pos1,pos2;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  pos1=vstream->data_start;
  pos2=opriv->total_bytes;
  //serialno=vstream->stream_id;

  start_pos = pos2 - BYTES_TO_READ;

  while (1) {
    if (start_pos<pos1) start_pos=pos1;

    //printf("in pos %ld and %ld with %ld\n",start_pos,pos2,vstream->data_start);

    page_pos = find_first_page(cdata, start_pos, pos2, &this_kframe, &this_frame);

    //printf("found page %ld %ld %ld\n",this_frame,last_frame,page_pos);

    if (this_frame == -1) {
      // no pages found in range
      if (last_frame >= 0) {     /* No more pages in range -> return last one */
        return last_frame;
      }
      if (start_pos <= pos1) {
        return -1;
      }
      /* Go back a bit */
      pos2-=start_pos;
      start_pos -= BYTES_TO_READ;
      if (start_pos<pos1) start_pos=pos1;
      pos2+=start_pos;
    } else {
      // found a page, see if we can find another one
      //printf("set last_frame to %ld\n",this_frame);
      last_frame = this_frame;
      start_pos=page_pos+1;
    }
  }
  return -1;
}


#ifdef HAVE_DIRAC

/* find first sync frame in a given file region. Return file offset. -1 is returned if no sync frame found.
 * pos2 in not included in the search.
 * This is for dirac only. */

static int64_t find_first_sync_frame(lives_clip_data_t *cdata, int64_t pos1, int64_t pos2, int64_t *frame) {

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  index_entry *idx=priv->idxc->idx;
  index_entry *extend=NULL;

  int64_t bytes;
  long result;

  ogg_t *opriv=priv->opriv;
  dirac_priv_t *dpriv=priv->dpriv;

  int pages_checked=0,page_packets_checked;

  size_t bytes_to_read=pos2-pos1+1;

  // check index to see if pos1 is in index

  while (idx!=NULL) {

    if (idx->pagepos==pos1 && idx->value>-1) {
      // found !
      *frame=idx->value;
      return pos1;
    }

    if (idx->pagepos<pos1&&idx->pagepos_end>pos1) {

      // region was partly checked
      // continue from pagepos_end + 1
      pos1=idx->pagepos_end+1;

      if (idx->pagepos_end>=pos2-1) {
        // region was checked and there are no seq frames
        *frame=-1;
        return pos1;
      }

      break;
    }

    if (idx->pagepos>=pos2) {
      // no entry found
      break;
    }

    idx=idx->next;
  }



  // pull pages and check packets until we either reach the end or find a sync point

  priv->input_position=pos1;

  ogg_stream_reset(&priv->vstream->stpriv->os);

  //printf ("start %ld %ld\n",pos1,pos2);

  seek_byte(cdata,pos1);

  if (bytes_to_read>BYTES_TO_READ) bytes_to_read=BYTES_TO_READ;

  while (1) {

    if (priv->input_position>=pos2) {
      // we reached the end and found no pages
      //printf("exit a\n");
      *frame=-1;
      return -1;
    }

    // read next chunk
    if (!(bytes=get_data(cdata,bytes_to_read))) {
      //printf("exit b\n");
      *frame=-1;
      return -1; // eof
    }

    bytes_to_read=BYTES_TO_READ;

    result = ogg_sync_pageseek(&opriv->oy, &opriv->current_page);

    //printf("result is %ld\n",result);

    if (result<0) {
      // found a page, sync to page start
      priv->input_position-=result;
      pos1=priv->input_position;
      continue;
    }

    if (result>0||(result==0&&opriv->oy.fill>3&&!strncmp((char *)opriv->oy.data,"OggS",4))) {
      pos1=priv->input_position;
      break;
    }

    priv->input_position+=bytes;

  };

  seek_byte(cdata,priv->input_position);
  ogg_stream_reset(&priv->vstream->stpriv->os);

  while (1) {

    //printf("here %ld and %ld\n",priv->input_position,pos2);

    if (priv->input_position>=pos2) {
      //printf("exit 1 %ld %ld %ld\n",priv->input_position,result,pos2);
      // reached the end of the search region and nothing was found
      *frame=-1;
      return priv->input_position; // we checked up to here
    }


    pthread_mutex_lock(&priv->idxc->mutex);

    if ((idx=find_pagepos_in_index(priv->idxc->idx,priv->input_position))!=NULL) {
      // this part was already checked

      //printf("WE ALREADY CHECKED THIS\n");

      if (idx->value!=-1) {
        // we already found a sync frame here
        if (extend!=NULL) {
          extend->pagepos_end=idx->pagepos-1;
          if (extend->pagepos_end<extend->pagepos) priv->idxc->idx=index_entry_delete(extend);
        }

        *frame=idx->value;
        pthread_mutex_unlock(&priv->idxc->mutex);
        //printf("As I recall, the sync frame was %ld\n",*frame);
        return idx->pagepos;
      }

      // no sync frame here - update this entry and jump to pagepos_end
      if (pages_checked>=2&&idx->pagepos<pos1) idx->pagepos=pos1;

      pos1=priv->input_position=idx->pagepos_end+1;
      pthread_mutex_unlock(&priv->idxc->mutex);

      //printf("no seq frames, skipping\n");

      // need more data
      ogg_stream_reset(&priv->vstream->stpriv->os);

      continue;
    }


    //printf("a: checking page\n");

    opriv->page_valid=0;

    if (!(result=get_page(cdata,priv->input_position))) {
      // EOF
      //printf("EOF ?\n");
      *frame=-1;
      return priv->input_position; // we checked up to here
    }

    // found a page
    if (priv->vstream->stream_id!=ogg_page_serialno(&(opriv->current_page))) {
      // page is not for this stream
      //printf("not for us\n");
      priv->input_position+=result;
      if (!pages_checked) pos1=priv->input_position;
      continue;
    }

    ogg_stream_pagein(&priv->vstream->stpriv->os, &(opriv->current_page));

    if (pages_checked>0) pages_checked++;

    page_packets_checked=0;

    while (ogg_stream_packetout(&priv->vstream->stpriv->os, &opriv->op) > 0) {

      //printf("a checking packet %c %c %c %c %d\n",opriv->op.packet[0],opriv->op.packet[1],opriv->op.packet[2],opriv->op.packet[3],opriv->op.packet[4]);

      page_packets_checked++;

      // is this packet a seq start ?
      if (opriv->op.packet[4]==0) {
        //printf("got a seq start\n");

        // if other packets ended on this page, we know that seq start begins on this page
        // otherwise it will have begun at pos1
        if (page_packets_checked>1) pos1=priv->input_position;

        // get the frame number, close extend, add a new index entry
        seek_byte(cdata,pos1);

        // feed packets to the dirac_decoder until we get a frame number
        // note: in theory we should be able to get the next frame from the granulepos
        // however this seems broken
        priv->last_frame=-1;
        priv->cframe=-1;

        ogg_data_process(cdata,NULL,FALSE);

        //printf("seq frame was %ld\n",priv->last_frame);

        *frame=priv->last_frame;

        schro_decoder_reset(dpriv->schrodec);

        ogg_stream_reset(&priv->vstream->stpriv->os);

        pthread_mutex_lock(&priv->idxc->mutex);
        if (priv->last_frame>-1) {
          // finish off the previous search area
          if (extend!=NULL) {
            extend->pagepos_end=pos1-1;
            if (extend->pagepos_end<extend->pagepos) priv->idxc->idx=index_entry_delete(extend);
          }

          // and add this one
          extend=dirac_index_entry_add(cdata,pos1,priv->input_position+result-1,*frame);

          if (extend->pagepos_end<extend->pagepos) priv->idxc->idx=index_entry_delete(extend);
          else if (extend->prev==NULL) priv->idxc->idx=extend;

        } else {
          // we got no frames, we must have reached EOF
          if (extend!=NULL) {
            extend->pagepos_end=priv->input_position-1;
            if (extend->pagepos_end<extend->pagepos) priv->idxc->idx=index_entry_delete(extend);
          }
          pos1=-1;
        }
        pthread_mutex_unlock(&priv->idxc->mutex);

        return pos1; // return offset of start page

      }

    }


    if (page_packets_checked>0) {
      // we got some other kind of packet out, so a sequence header packet could only begin on this page
      pos1=priv->input_position; // the start of this page

      if (pages_checked==-1) {
        // we can start counting pages from here
        pages_checked=1;
      }


      if (pages_checked>=2) {
        // if we checked from one packet to the next and found no seq header.
        // We now know that the first pages had no seq header
        // so we can start marking from pos1 up to start of this page - 1
        pthread_mutex_lock(&priv->idxc->mutex);
        if (extend==NULL) {
          extend=dirac_index_entry_add(cdata,pos1,priv->input_position-1,-1);
          if (extend->prev==NULL) priv->idxc->idx=extend;
        } else {
          extend->pagepos_end=priv->input_position-1;
        }
        if (extend->pagepos_end<extend->pagepos) priv->idxc->idx=index_entry_delete(extend);
        else if (extend->prev!=NULL&&extend->prev->pagepos_end==extend->pagepos-1&&extend->prev->value==-1) {
          extend->prev->pagepos_end=extend->pagepos_end;
          index_entry_delete(extend);
        }
        pthread_mutex_unlock(&priv->idxc->mutex);
      }

    }

    //  -> start of next page
    priv->input_position+=result;

  }
}




/* find last sync frame position in a given file region. Return file offset. -1 is returned if no sync frame found.
This is for dirac only. */


static int64_t find_last_sync_frame(lives_clip_data_t *cdata, lives_in_stream *vstream) {
  int64_t page_pos, last_page_pos = -1, start_pos;
  int64_t this_frame=-1;
  int64_t pos1,pos2;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  pos1=vstream->data_start;
  pos2=opriv->total_bytes;
  //serialno=vstream->stream_id;

  start_pos = pos2 - BYTES_TO_READ;

  while (1) {
    if (start_pos<pos1) start_pos=pos1;

    page_pos = find_first_sync_frame(cdata, start_pos, pos2, &this_frame);

    //printf("b found sync at %ld %ld\n",this_frame,page_pos);

    if (this_frame == -1) {
      // no sync frames found in range
      if (last_page_pos >= 0) {   /* No more pages in range -> return last one */
        return last_page_pos;
      }
      if (start_pos <= pos1) {
        return -1;
      }
      /* Go back a bit */
      pos2-=start_pos;
      start_pos -= BYTES_TO_READ;
      if (start_pos<pos1) start_pos=pos1;
      pos2+=start_pos;
    } else {
      // found a page, see if we can find another one
      last_page_pos = page_pos;
      start_pos = page_pos+1;
    }
  }
  return -1;
}


#endif








static int64_t get_last_granulepos(lives_clip_data_t *cdata, int serialno) {
  // granulepos here is actually a frame - TODO ** fix for audio !

#ifdef HAVE_DIRAC
  int64_t pos;
#endif
  lives_in_stream *stream;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  int64_t frame;

  stream=stream_from_sno(cdata,serialno);
  if (stream==NULL) return -1;

#ifdef HAVE_DIRAC
  if (stream->stpriv->fourcc_priv==FOURCC_DIRAC) {
    dirac_priv_t *dpriv=priv->dpriv;

    pos=find_last_sync_frame(cdata,priv->vstream);
    seek_byte(cdata,pos);

    ogg_stream_reset(&priv->vstream->stpriv->os);

    schro_decoder_reset(dpriv->schrodec);

    priv->last_frame=-1;
    priv->cframe=-1;

    ogg_data_process(cdata,NULL,FALSE); // read pages from last sync frame

    while (ogg_data_process(cdata,NULL,TRUE)); // continue until we reach EOF

    schro_decoder_reset(dpriv->schrodec);

    ogg_stream_reset(&priv->vstream->stpriv->os);

    //printf("last dirac frame is %ld, adjust to %ld\n",priv->last_frame, priv->last_frame-priv->kframe_offset);

    priv->last_frame-=priv->kframe_offset;

    return priv->last_frame;
  }
#endif

  // TODO - fix for vorbis

  frame = find_last_theora_frame(cdata,priv->vstream);
  if (frame < 0) return -1;

  return frame;

}



static double granulepos_2_time(lives_in_stream *s, int64_t pos) {
  // actually should be frame or sample to time

  int64_t frames;
  stream_priv_t *stream_priv = (stream_priv_t *)(s->stpriv);

  switch (stream_priv->fourcc_priv) {
  case FOURCC_VORBIS:
    //fprintf(stderr,"apos is %lld\n",pos);
    return ((long double)pos/(long double)s->samplerate);
    break;
  case FOURCC_THEORA:
  case FOURCC_DIRAC:
    //fprintf(stderr,"vpos is %lld\n",pos);
    stream_priv = (stream_priv_t *)(s->stpriv);

    frames = pos;

    return ((double)s->fps_denom/(double)s->fps_num*(double)frames);
    break;

  }
  return -1;
}


static ssize_t stream_peek(int fd, char *str, size_t len) {
  off_t cpos=lseek(fd,0,SEEK_CUR); // get current posn

#ifndef IS_MINGW
  return pread(fd,str,len,cpos); // read len bytes without changing cpos
#else
  ssize_t ret=read(fd,str,len);
  lseek(fd,cpos,SEEK_SET);
  return ret;
#endif
}




static int open_ogg(lives_clip_data_t *cdata) {
  char scheck[4];

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  if (stream_peek(opriv->fd,scheck,4)<4) return 0;

  if (strncmp(scheck,"OggS",4)) return 0;

  ogg_sync_init(&(opriv->oy));


  /* Set up the tracks */
  if (!setup_tracks(cdata)) {
    ogg_sync_clear(&opriv->oy);
    return 0;
  }


  return 1;
}





static boolean attach_stream(lives_clip_data_t *cdata, boolean isclone) {
  // open the file and get a handle

  struct stat sb;

  int64_t gpos;

  double stream_duration;

  boolean is_partial_clone=FALSE;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv=(ogg_t *)malloc(sizeof(ogg_t));

  register int i;

  if ((opriv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "ogg_theora_decoder: unable to open %s\n",cdata->URI);
    free(opriv);
    priv->opriv=NULL;
    return FALSE;
  }

#ifdef IS_MINGW
  setmode(opriv->fd,O_BINARY);
#endif

  if (isclone&&!priv->inited) {
    isclone=FALSE;
    if (cdata->fps>0.&&cdata->nframes>0)
      is_partial_clone=TRUE;
  }

  if (!isclone) {
    stat(cdata->URI,&sb);
    opriv->total_bytes=sb.st_size;
  }

  opriv->page_valid=0;

  // index init
  priv->idxc=idxc_for(cdata);

  /* get ogg info */
  if (!open_ogg(cdata)) {
    close(opriv->fd);
    free(opriv);
    priv->opriv=NULL;
    return FALSE;
  }


  priv->last_kframe=10000000;
  priv->last_frame=100000000;
  priv->inited=TRUE;


#ifdef HAVE_THEORA
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
    // theora only
    int i;
    ogg_packet op;
    uint8_t *ext_pos;
    theora_priv_t *tpriv=priv->tpriv=(theora_priv_t *)malloc(sizeof(theora_priv_t));

    /* Initialize theora structures */
    theora_info_init(&tpriv->ti);
    theora_comment_init(&tpriv->tc);

    /* Get header packets and initialize decoder */
    if (priv->vstream->ext_data==NULL) {
      fprintf(stderr, "Theora codec requires extra data\n");
      close(opriv->fd);
      free(opriv);
      priv->opriv=NULL;
      free(tpriv);
      priv->tpriv=NULL;
      return FALSE;
    }

    ext_pos = priv->vstream->ext_data;

    for (i=0; i<3; i++) {
      ext_pos = ptr_2_op(ext_pos, &op);
      theora_decode_header(&tpriv->ti, &tpriv->tc, &op);
    }

    theora_decode_init(&tpriv->ts, &tpriv->ti);

    // check to find whether first frame is zero or one
    // if version >= 3.2.1 first kframe is 1; otherwise 0
    if (priv->vstream->version>=3002001) priv->kframe_offset=1;
    else priv->kframe_offset=0;
  }
#endif


#ifdef HAVE_DIRAC
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
    // dirac only
    dirac_priv_t *dpriv=priv->dpriv=(dirac_priv_t *)malloc(sizeof(dirac_priv_t));

    schro_init();

    dpriv->schroframe=NULL;

    if ((dpriv->schrodec=schro_decoder_new())==NULL) {
      fprintf(stderr, "Failed to init schro decoder\n");
      close(opriv->fd);
      free(opriv);
      priv->opriv=NULL;
      free(dpriv);
      priv->dpriv=NULL;
      return FALSE;
    }
    //schro_debug_set_level(SCHRO_LEVEL_WARNING);
    schro_debug_set_level(0);

  }
#endif

  if (isclone) return TRUE;

  cdata->nclips=1;

  // video part
  cdata->interlace=LIVES_INTERLACE_NONE;

  cdata->par=1.;

  // TODO
  cdata->offs_x=0;
  cdata->offs_y=0;
  cdata->frame_width=cdata->width;
  cdata->frame_height=cdata->height;

  cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;
  cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
  cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;



  /* Get format */

#ifdef HAVE_THEORA
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
    theora_priv_t *tpriv=priv->tpriv;
    cdata->width  = cdata->frame_width = opriv->y_width = tpriv->ti.frame_width;
    cdata->height = cdata->frame_height = opriv->y_height = tpriv->ti.frame_height;

    if (cdata->fps==0.) cdata->fps  = (float)tpriv->ti.fps_numerator/(float)tpriv->ti.fps_denominator;

    switch (tpriv->ti.pixelformat) {
    case OC_PF_420:
      cdata->palettes[0] = WEED_PALETTE_YUV420P;
      opriv->uv_width = opriv->y_width>>1;
      break;
    case OC_PF_422:
      cdata->palettes[0] = WEED_PALETTE_YUV422P;
      opriv->uv_width = opriv->y_width>>1;
      break;
    case OC_PF_444:
      cdata->palettes[0] = WEED_PALETTE_YUV444P;
      opriv->uv_width = opriv->y_width;
      break;
    default:
      fprintf(stderr, "Unknown pixelformat %d", tpriv->ti.pixelformat);
      return FALSE;
    }

    cdata->palettes[1]=WEED_PALETTE_END;
    cdata->current_palette=cdata->palettes[0];
    sprintf(cdata->video_name,"%s","theora");

    if (tpriv->tc.comments>0) {
      size_t lenleft=256,csize;
      char *cbuf=cdata->comment;
      for (i=0; i<=tpriv->tc.comments; i++) {
        csize=tpriv->tc.comment_lengths[i];
        if (csize>lenleft) csize=lenleft;
        snprintf(cbuf,csize,"%s",tpriv->tc.user_comments[i]);
        cbuf+=csize;
        lenleft-=csize;
        if (lenleft==0) break;
        if (i+1<=tpriv->tc.comments) {
          if (lenleft<strlen("\n")+1) break;
          sprintf(cbuf,"\n");
          cbuf+=strlen("\n");
          lenleft-=strlen("\n");
        }
      }
    }

  }
#endif


#ifdef HAVE_DIRAC
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
    // feed some pages to the decoder so it gets the idea
    dirac_priv_t *dpriv=priv->dpriv;
    int64_t start_pos=priv->input_position;

    seek_byte(cdata,priv->vstream->data_start);

    while (1) {
      int64_t input_pos;
      boolean done=FALSE;

      opriv->page_valid=0;

      if (!(input_pos=get_page(cdata,priv->input_position))) {
        // should never reach here
        fprintf(stderr, "EOF1 while decoding\n");
        return FALSE;
      }

      priv->input_position+=input_pos;

      if (ogg_page_serialno(&(opriv->current_page))!=priv->vstream->stream_id) continue;
      ogg_stream_pagein(&priv->vstream->stpriv->os, &(opriv->current_page));

      while (ogg_stream_packetout(&priv->vstream->stpriv->os, &opriv->op) > 0) {
        // feed packets to decoder
        SchroBuffer *schrobuffer;
        int state;

        schrobuffer = schro_buffer_new_with_data(opriv->op.packet, opriv->op.bytes);
        schro_decoder_autoparse_push(dpriv->schrodec, schrobuffer);

        state = schro_decoder_autoparse_wait(dpriv->schrodec);

        if (state==SCHRO_DECODER_FIRST_ACCESS_UNIT) {
          // should have our cdata now
          get_dirac_cdata(cdata,dpriv->schrodec);
          done=TRUE;
          break;
        }
        if (done) break;
      }
      if (done) break;
    }

    cdata->current_palette=cdata->palettes[0];

    // reset and seek back to start
    //fprintf(stderr,"got dirac fps=%.4f %d x %d\n",cdata->fps,cdata->width,cdata->height);


    schro_decoder_reset(dpriv->schrodec);
    seek_byte(cdata,start_pos);
    ogg_stream_reset(&priv->vstream->stpriv->os);

    // find first keyframe
    find_first_sync_frame(cdata, priv->vstream->data_start, opriv->total_bytes, &priv->kframe_offset);

    sprintf(cdata->video_name,"%s","dirac");

  }

#endif


  sprintf(cdata->container_name,"%s","ogg");


  // audio part
  cdata->asigned=FALSE;
  cdata->achans=0;
  cdata->ainterleaf=TRUE;
  cdata->asamps=0;

  if (priv->astream!=NULL) {
    sprintf(cdata->audio_name,"%s","vorbis");
  } else memset(cdata->audio_name,0,1);


  if (isclone) return TRUE;

  // get duration


  if (priv->astream!=NULL) {
    ogg_stream_reset(&priv->astream->stpriv->os);
    gpos=get_last_granulepos(cdata,priv->astream->stream_id);
    stream_duration =
      granulepos_2_time(priv->astream,gpos);
    priv->astream->duration=stream_duration;
    priv->astream->stpriv->last_granulepos=gpos;
    //printf("priv->astream duration is %.4f\n",stream_duration);
  }

  if (priv->vstream!=NULL) {
    ogg_stream_reset(&priv->vstream->stpriv->os);
    gpos=get_last_granulepos(cdata,priv->vstream->stream_id);

    /*  kframe=gpos >> priv->vstream->priv->keyframe_granule_shift;
    priv->vstream->nframes = kframe + gpos-(kframe<<priv->vstream->priv->keyframe_granule_shift);*/

    priv->vstream->nframes=gpos;

    stream_duration =
      granulepos_2_time(priv->vstream,gpos);
    priv->vstream->duration=stream_duration;
    priv->vstream->stpriv->last_granulepos=gpos;
    //printf("priv->vstream duration is %.4f %ld\n",stream_duration,priv->vstream->nframes);
  }

  if (is_partial_clone) return TRUE;


  cdata->nframes=priv->vstream->nframes;

  if (cdata->width!=cdata->frame_width||cdata->height!=cdata->frame_height)
    fprintf(stderr,"ogg_decoder: info - frame size=%d x %d, pixel size=%d x %d\n",cdata->frame_width,cdata->frame_height,cdata->width,
            cdata->height);


  return TRUE;
}




static void detach_stream(lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

#ifdef HAVE_DIRAC
  dirac_priv_t *dpriv=priv->dpriv;
#endif

  close(opriv->fd);

  ogg_sync_clear(&opriv->oy);

#ifdef HAVE_THEORA
  if (priv->tpriv!=NULL) {
    // theora only
    // TODO - call function in decoder
    theora_priv_t *tpriv=priv->tpriv;
    theora_clear(&tpriv->ts);
    theora_comment_clear(&tpriv->tc);
    theora_info_clear(&tpriv->ti);
    free(tpriv);
    priv->tpriv=NULL;
  }
#endif


#ifdef HAVE_DIRAC
  if (priv->dpriv!=NULL) {
    schro_decoder_reset(dpriv->schrodec);
    if (dpriv->schroframe!=NULL) {
      schro_frame_unref(dpriv->schroframe);
    }
    if (dpriv->schrodec!=NULL) schro_decoder_free(dpriv->schrodec);
    dpriv->schrodec=NULL;
    dpriv->schroframe=NULL;
    free(dpriv);
    priv->dpriv=NULL;
  }
#endif

  // free stream data
  if (priv->astream!=NULL) {
    if (priv->astream->ext_data!=NULL) free(priv->astream->ext_data);
    ogg_stream_clear(&priv->astream->stpriv->os);
    free(priv->astream->stpriv);
    free(priv->astream);
    priv->astream=NULL;
  }

  if (priv->vstream!=NULL) {
    if (priv->vstream->ext_data!=NULL) free(priv->vstream->ext_data);
    ogg_stream_clear(&priv->vstream->stpriv->os);
    free(priv->vstream->stpriv);
    free(priv->vstream);
    priv->vstream=NULL;
  }


  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;


}


static inline int64_t frame_to_gpos(lives_clip_data_t *cdata, int64_t kframe, int64_t frame) {
  // this is only valid for theora; for dirac there is no unique gpos for a particular frame
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
    return kframe;
  }
  return (kframe << priv->vstream->stpriv->keyframe_granule_shift) + (frame-kframe);
}


static int64_t ogg_seek(lives_clip_data_t *cdata, int64_t tframe, int64_t ppos_lower, int64_t ppos_upper, boolean can_exact) {


  // for theora:

  // we do two passes here, first with can_exact set, then with can_exact unset

  // if can_exact is set, we find the granulepos nearest to or including the target granulepos
  // from this we extract an estimate of the keyframe (but note that there could be other keyframes between the found granulepos and the target)

  // on the second pass we find the highest granulepos < target. This places us just before or at the start of the target keyframe

  // when we come to decode, we start from this second position, discarding any completed packets on that page,
  // and read pages discarding packets until we get to the target frame

  // we return the granulepos which we find




  // for dirac:

  // we find the highest sync frame <= target frame, and return the sync_frame number
  // can_exact should be set to TRUE



  // the method used is bi-sections
  // first we divided the region into two, then we check the first keyframe of the upper part
  // if this is == target we return
  // if > target, or we find no keyframes, we go to the lower segment
  // this is then repeated until the segment size is too small to hold a packet, at which point we return our best
  // match


  int64_t start_pos,end_pos,pagepos=-1;
  int64_t frame,kframe,segsize;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  int64_t best_kframe=-1;
  int64_t best_frame=-1;
  int64_t best_pagepos=-1;

  //fprintf(stderr,"seek to frame %ld %d\n",tframe,can_exact);

  if (tframe<priv->kframe_offset) {
    priv->cpagepos=priv->vstream->data_start;
    if (!can_exact) {
      seek_byte(cdata,priv->vstream->data_start);
      return frame_to_gpos(cdata,priv->kframe_offset,1);
    }
    return frame_to_gpos(cdata,priv->kframe_offset,0);
  }

  if (ppos_lower<0) ppos_lower=priv->vstream->data_start;
  if (ppos_upper<0) ppos_upper=opriv->total_bytes;
  if (ppos_upper>opriv->total_bytes) ppos_upper=opriv->total_bytes;

  start_pos=ppos_lower;
  end_pos=ppos_upper;

  segsize=(end_pos-start_pos+1)>>1;

  do {
    // see if the frame lies in current segment

    if (start_pos<ppos_lower) start_pos=ppos_lower;
    if (end_pos>ppos_upper) end_pos=ppos_upper;

    if (start_pos>=end_pos) {
      if (start_pos==ppos_lower) {
        if (!can_exact) seek_byte(cdata,start_pos);
        priv->cpagepos=start_pos;
        return frame_to_gpos(cdata,priv->kframe_offset,1);
      }
      break;
    }

    //fprintf(stderr,"check seg %ld to %ld for %ld\n",start_pos,end_pos,tframe);

    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) pagepos=find_first_page(cdata,start_pos,end_pos,&kframe,&frame);
#ifdef HAVE_DIRAC
    else {
      pagepos=find_first_sync_frame(cdata,start_pos,end_pos,&kframe);
      frame=kframe;
    }
#endif
    if (pagepos!=-1&&kframe!=-1) {
      // found a keyframe

      //fprintf(stderr,"first sync_frame is %ld %ld\n",kframe,frame);

      if (can_exact&&frame>=tframe&&kframe<=tframe) {
        // got it !
        //fprintf(stderr,"seek got keyframe %ld for %ld\n",kframe,tframe);
        priv->cpagepos=pagepos;
        return frame_to_gpos(cdata,kframe,frame);
      }

      if ((kframe<tframe||(can_exact&&kframe==tframe))&&kframe>best_kframe) {
        best_kframe=kframe;
        best_frame=frame;
        best_pagepos=pagepos;
      }

      if (frame>=tframe) {
        //fprintf(stderr,"checking lower segment\n");
        // check previous segment
        start_pos-=segsize;
        end_pos-=segsize;
      } else start_pos=pagepos;

    }

    // no keyframe found, check lower segment
    else {
      //fprintf(stderr,"no keyframe, checking lower segment\n");
      end_pos-=segsize;
      start_pos-=segsize;
    }

    segsize=(end_pos-start_pos+1)>>1;
    start_pos+=segsize;
  } while (segsize>64);

  if (best_kframe>-1) {
    int64_t gpos=frame_to_gpos(cdata,best_kframe,tframe);
    //fprintf(stderr,"seek2 got keyframe %ld %ld\n",best_kframe,best_frame);
    if (!can_exact) seek_byte(cdata,best_pagepos);
    priv->cpagepos=best_pagepos;
    // TODO - add to vlc
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
      pthread_mutex_lock(&priv->idxc->mutex);
      theora_index_entry_add((lives_clip_data_t *)cdata,gpos,priv->cpagepos);
      pthread_mutex_unlock(&priv->idxc->mutex);
    }
    gpos=frame_to_gpos(cdata,best_kframe,best_frame);
    return gpos;
  }

  // should never reach here
  //fprintf(stderr,"not found\n");
  return -1;
}




//////////////////////////////////////////
// std functions



const char *version(void) {
  return plugin_version;
}


const char *module_check_init(void) {
  indices=NULL;
  nidxc=0;
  pthread_mutex_init(&indices_mutex,NULL);
  return NULL;
}


lives_clip_data_t *init_cdata(void) {
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));
  lives_ogg_priv_t *priv=malloc(sizeof(lives_ogg_priv_t));;


  cdata->priv=priv;

  priv->vstream=NULL;
  priv->astream=NULL;
  priv->inited=FALSE;

  priv->opriv=NULL;

#ifdef HAVE_THEORA
  priv->tpriv=NULL;
#endif

#ifdef HAVE_DIRAC
  priv->dpriv=NULL;
#endif

  priv->idxc=NULL;

  cdata->video_start_time=0.;
  cdata->sync_hint=0;

  cdata->fps=0.;

  cdata->palettes=malloc(2*sizeof(int));
  cdata->palettes[1]=WEED_PALETTE_END;

  cdata->URI=NULL;

  memset(cdata->author,0,1);
  memset(cdata->title,0,1);
  memset(cdata->comment,0,1);

  return cdata;
}


static lives_clip_data_t *ogg_clone(lives_clip_data_t *cdata) {
  lives_clip_data_t *clone=init_cdata();
  lives_ogg_priv_t *dpriv,*spriv;

  ogg_t *dopriv;
  ogg_t *sopriv;

  // copy from cdata to clone, with a new context for clone
  clone->URI=strdup(cdata->URI);
  clone->nclips=cdata->nclips;
  snprintf(clone->container_name,512,"%s",cdata->container_name);
  clone->current_clip=cdata->current_clip;
  clone->width=cdata->width;
  clone->height=cdata->height;
  clone->nframes=cdata->nframes;
  clone->interlace=cdata->interlace;
  clone->offs_x=cdata->offs_x;
  clone->offs_y=cdata->offs_y;
  clone->frame_width=cdata->frame_width;
  clone->frame_height=cdata->frame_height;
  clone->par=cdata->par;
  clone->fps=cdata->fps;
  if (cdata->palettes!=NULL) clone->palettes[0]=cdata->palettes[0];
  clone->current_palette=cdata->current_palette;
  clone->YUV_sampling=cdata->YUV_sampling;
  clone->YUV_clamping=cdata->YUV_clamping;
  snprintf(clone->video_name,512,"%s",cdata->video_name);
  clone->arate=cdata->arate;
  clone->achans=cdata->achans;
  clone->asamps=cdata->asamps;
  clone->asigned=cdata->asigned;
  clone->ainterleaf=cdata->ainterleaf;
  snprintf(clone->audio_name,512,"%s",cdata->audio_name);
  clone->seek_flag=cdata->seek_flag;
  clone->sync_hint=cdata->sync_hint;
  clone->video_start_time=cdata->video_start_time;

  snprintf(clone->author,256,"%s",cdata->author);
  snprintf(clone->title,256,"%s",cdata->title);
  snprintf(clone->comment,256,"%s",cdata->comment);

  // create "priv" elements
  dpriv=clone->priv;
  spriv=cdata->priv;

  if (spriv!=NULL) {
    sopriv=spriv->opriv;
    dopriv=dpriv->opriv;
    dpriv->inited=TRUE;
    dopriv->total_bytes=sopriv->total_bytes;
  }

  if (!attach_stream(clone,TRUE)) {
    free(clone->URI);
    clone->URI=NULL;
    clip_data_free(clone);
    return NULL;
  }

  if (spriv!=NULL) {
    ogg_stream_reset(&dpriv->astream->stpriv->os);
    dpriv->astream->duration=spriv->astream->duration;
    dpriv->astream->stpriv->last_granulepos=spriv->astream->stpriv->last_granulepos;

    ogg_stream_reset(&dpriv->vstream->stpriv->os);
    dpriv->vstream->nframes=spriv->vstream->nframes;
    dpriv->vstream->duration=spriv->vstream->duration;
    dpriv->vstream->stpriv->last_granulepos=spriv->vstream->stpriv->last_granulepos;

  }

  dpriv->last_frame=-1;

  return clone;
}





lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the clone
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI, a different current_clip, or a different current_palette,
  // this must be called again with the same
  // cdata as the second parameter
  lives_ogg_priv_t *priv;

  if (URI==NULL&&cdata!=NULL) {
    // create a clone of cdata - we also need to be able to handle a "fake" clone with only URI, nframes and fps set (priv == NULL)
    return ogg_clone(cdata);
  }

  if (cdata!=NULL&&cdata->current_clip>0) {
    // currently we only support one clip per container

    clip_data_free(cdata);
    return NULL;
  }

  if (cdata==NULL) {
    cdata=init_cdata();
  }

  priv=(lives_ogg_priv_t *)cdata->priv;

  if (cdata->URI==NULL||strcmp(URI,cdata->URI)) {
    if (cdata->URI!=NULL) {
      detach_stream(cdata);
      free(cdata->URI);
    }
    cdata->URI=strdup(URI);
    if (!attach_stream(cdata,FALSE)) {
      free(cdata->URI);
      cdata->URI=NULL;
      clip_data_free(cdata);
      return NULL;
    }
    cdata->current_clip=0;
  }

  if (priv->vstream==NULL) {
    clip_data_free(cdata);
    return NULL;
  }


  return cdata;
}


#ifdef HAVE_THEORA

static boolean ogg_theora_read(lives_clip_data_t *cdata, ogg_packet *op, yuv_buffer *yuv) {
  // this packet is for our theora decoder
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  theora_priv_t *tpriv=priv->tpriv;

  //printf("skip is %d\n",priv->skip);

  if (theora_decode_packetin(&tpriv->ts,op)==0) {
    if (priv->skip<=0) {
      if (theora_decode_YUVout(&tpriv->ts, yuv)==0) {
        priv->frame_out=TRUE;
      }
    }
  }
  return priv->frame_out;
}

#endif

#ifdef HAVE_THEORA

static size_t write_black_pixel(unsigned char *idst, int pal, int npixels, int y_black) {
  unsigned char *dst=idst;
  register int i;

  for (i=0; i<npixels; i++) {
    switch (pal) {
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
      dst[0]=dst[1]=dst[2]=0;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_ARGB32:
      dst[1]=dst[2]=dst[3]=0;
      dst[0]=255;
      dst+=4;
      break;
    case WEED_PALETTE_UYVY8888:
      dst[1]=dst[3]=y_black;
      dst[0]=dst[2]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUYV8888:
      dst[0]=dst[2]=y_black;
      dst[1]=dst[3]=128;
      dst+=4;
      break;
    case WEED_PALETTE_YUV888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst+=3;
      break;
    case WEED_PALETTE_YUVA8888:
      dst[0]=y_black;
      dst[1]=dst[2]=128;
      dst[3]=255;
      dst+=4;
      break;
    case WEED_PALETTE_YUV411:
      dst[0]=dst[3]=128;
      dst[1]=dst[2]=dst[4]=dst[5]=y_black;
      dst+=6;
    default:
      break;
    }
  }
  return idst-dst;
}
#endif


#ifdef HAVE_DIRAC


static void schroframe_to_pixel_data(SchroFrame *sframe, uint8_t **pixel_data) {
  uint8_t *s_y=sframe->components[0].data;
  uint8_t *s_u=sframe->components[1].data;
  uint8_t *s_v=sframe->components[2].data;

  uint8_t *d_y=pixel_data[0];
  uint8_t *d_u=pixel_data[1];
  uint8_t *d_v=pixel_data[2];

  register int i;

  boolean crow=FALSE;

  int mheight=(sframe->components[0].height >> 1) << 1;

  for (i=0; i<mheight; i++) {
    memcpy(d_y,s_y,sframe->components[0].width);
    d_y+=sframe->components[0].width;
    s_y+=sframe->components[0].stride;
    if (sframe->components[1].height==sframe->components[0].height||crow) {
      memcpy(d_u,s_u,sframe->components[1].width);
      memcpy(d_v,s_v,sframe->components[2].width);
      d_u+=sframe->components[1].width;
      d_v+=sframe->components[2].width;
      s_u+=sframe->components[1].stride;
      s_v+=sframe->components[2].stride;
    }
    crow=!crow;
    sched_yield();
  }
}






static int64_t ogg_dirac_read(lives_clip_data_t *cdata, ogg_packet *op, uint8_t **pixel_data, boolean cont) {
  // this packet is for our dirac decoder
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  dirac_priv_t *dpriv=priv->dpriv;
  SchroDecoder *sd=dpriv->schrodec;
  SchroBuffer *schrobuffer;
  SchroFrame *schroframe;
  int state;
  int frame_width;
  int frame_height;

  int i;

  void *buf;

  if (!cont) {
    buf=malloc(op->bytes);
    memcpy(buf,op->packet,op->bytes);

    schrobuffer = schro_buffer_new_with_data(buf, op->bytes);
    schrobuffer->priv=buf;

    schrobuffer->free = SchroBufferFree;

    if (!schro_decoder_push_ready(sd)) fprintf(stderr,"decoder not ready !\n");

    schro_decoder_autoparse_push(sd, schrobuffer);
  }

  while (1) {

    state = schro_decoder_autoparse_wait(sd);
    switch (state) {

    case SCHRO_DECODER_EOS:
      printf("EOS\n");
      continue;

    case SCHRO_DECODER_FIRST_ACCESS_UNIT:
      // TODO - re-parse the video format
      //printf("fau\n");
      continue;

    case SCHRO_DECODER_NEED_BITS:
      //printf("need bits\n");
      return priv->frame_out;

    case SCHRO_DECODER_NEED_FRAME:
      //printf("need frame\n");
      schroframe = schro_frame_new();
      schro_frame_set_free_callback(schroframe, SchroFrameFree, NULL);

      if (cdata->current_palette==WEED_PALETTE_YUV420P) schroframe->format = SCHRO_FRAME_FORMAT_U8_420;
      if (cdata->current_palette==WEED_PALETTE_YUV422P) schroframe->format = SCHRO_FRAME_FORMAT_U8_422;
      if (cdata->current_palette==WEED_PALETTE_YUV444P) schroframe->format = SCHRO_FRAME_FORMAT_U8_444;

      schroframe->width=frame_width=cdata->frame_width;
      schroframe->height=frame_height=cdata->frame_height;

      for (i=0; i<3; i++) {
        schroframe->components[i].width = frame_width;
        schroframe->components[i].stride = frame_width;
        schroframe->components[i].height = frame_height;
        schroframe->components[i].length = frame_width*frame_height;
        schroframe->components[i].data = malloc(frame_width*frame_height);

        if (i==0) {
          if (cdata->current_palette==WEED_PALETTE_YUV420P) {
            frame_width>>=1;
            frame_height>>=1;
          }
          if (cdata->current_palette==WEED_PALETTE_YUV422P) {
            frame_width>>=1;
          }

        } else {
          schroframe->components[i].v_shift =
            SCHRO_FRAME_FORMAT_V_SHIFT(schroframe->format);
          schroframe->components[i].h_shift =
            SCHRO_FRAME_FORMAT_H_SHIFT(schroframe->format);
        }
      }

      schro_decoder_add_output_picture(sd, schroframe);
      break;

    case SCHRO_DECODER_OK:
      priv->last_frame=schro_decoder_get_picture_number(sd);
      //else priv->last_frame++;
      //fprintf(stderr,"dirac decoding picture %ld skip is %d, target %ld\n",priv->last_frame,priv->skip,priv->cframe);
      if ((schroframe=schro_decoder_pull(sd))!=NULL) {
        if (priv->cframe<0||priv->cframe==priv->last_frame) {
          priv->frame_out=TRUE;
          // copy data to pixel_data
          if (pixel_data!=NULL) {
            schroframe_to_pixel_data(schroframe,pixel_data);
            dpriv->schroframe=schroframe;
            return TRUE;
          }
        }
        schro_frame_unref(schroframe);
        //priv->cframe++;
        //priv->skip--;
      }
      //else printf("Null frame !\n");
      if (pixel_data==NULL) return TRUE;
      break;

    case SCHRO_DECODER_ERROR:
      fprintf(stderr,"Schro decoder error !\n");
      priv->frame_out=TRUE;
      return priv->frame_out;
    }
  }

  return priv->frame_out;
}

#endif





static boolean ogg_data_process(lives_clip_data_t *cdata, void *yuvbuffer, boolean cont) {
  // yuvbuffer must be cast to whatever decoder expects

  int64_t input_pos=0;
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  priv->frame_out=FALSE;

  if (!cont) ogg_stream_reset(&priv->vstream->stpriv->os);

#ifdef HAVE_DIRAC
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
    priv->ignore_packets=FALSE;
    if (!cont) {
      dirac_priv_t *dpriv=priv->dpriv;
      schro_decoder_reset(dpriv->schrodec);
      priv->last_frame=-1;
    }
  }
#endif

  while (!priv->frame_out) {
    opriv->page_valid=0;

    if (!cont) {
      if (!(input_pos=get_page(cdata,priv->input_position))) {
        // should never reach here (except when discovering last frame for dirac)
        //fprintf(stderr, "EOF1 while decoding\n");
        return FALSE;
      }
      priv->input_position+=input_pos;

      if (ogg_page_serialno(&(opriv->current_page))!=priv->vstream->stream_id) continue;
      ogg_stream_pagein(&priv->vstream->stpriv->os, &(opriv->current_page));
      //printf("pulled page\n");
    }

    while ((cont&&priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) ||
           ogg_stream_packetout(&priv->vstream->stpriv->os, &opriv->op) > 0) {

      // cframe is the frame we are about to decode

      //fprintf(stderr,"cframe is %ld\n",priv->cframe);

#ifdef HAVE_THEORA
      if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {

        if (!priv->ignore_packets) {
          yuv_buffer *yuv=yuvbuffer;
          ogg_theora_read(cdata,&opriv->op,yuv);
        }

        priv->cframe++;
        priv->skip--;


      }
#endif

#ifdef HAVE_DIRAC
      if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
        uint8_t **yuv=yuvbuffer;

        // TODO - if packet is seq start, add to index
        //printf("dec packet\n");

        ogg_dirac_read(cdata,&opriv->op,yuv, cont);

        // dirac can pull several frames from the same packet
        if (!priv->frame_out) cont=FALSE;
      }
#endif

      if (priv->frame_out) break;

      sched_yield();
    }
    priv->ignore_packets=FALSE; // start decoding packets

    cont=FALSE;
  }

  return TRUE;
}


boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, int *rowstrides, int height, void **pixel_data) {
  // seek to frame, and return pixel_data

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

#ifdef HAVE_THEORA
  yuv_buffer yuv;
  register int i;
#endif

#ifdef HAVE_DIRAC
  dirac_priv_t *dpriv=priv->dpriv;
  ogg_t *opriv=priv->opriv;
#endif

  int64_t kframe=-1,xkframe;
  boolean cont=FALSE;
  int max_frame_diff;
  int64_t granulepos=0;
  int64_t ppos_lower=-1,ppos_upper=-1;

  static index_entry *fidx=NULL;

#ifdef HAVE_THEORA

  register int p;

  unsigned char *dst,*src;

  int y_black=(cdata->YUV_clamping==WEED_YUV_CLAMPING_CLAMPED)?16:0;
  unsigned char black[4]= {0,0,0,255};

  int xheight=(cdata->frame_height>>1)<<1,pal=cdata->current_palette,nplanes=1,dstwidth=cdata->width,psize=1;
  int btop=cdata->offs_y,bbot=xheight-1-btop;
  int bleft=cdata->offs_x,bright=cdata->frame_width-cdata->width-bleft;

  if (pixel_data!=NULL) {
    if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) {
      nplanes=3;
      black[0]=y_black;
      black[1]=black[2]=128;
    } else if (pal==WEED_PALETTE_YUVA4444P) {
      nplanes=4;
      black[0]=y_black;
      black[1]=black[2]=128;
      black[3]=255;
    }

    if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) psize=3;

    if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||
        pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888) psize=4;

    if (pal==WEED_PALETTE_YUV411) psize=6;

    if (pal==WEED_PALETTE_A1) dstwidth>>=3;

    dstwidth*=psize;

    if (cdata->frame_height > cdata->height && height == cdata->height) {
      // host ignores vertical border
      btop=0;
      xheight=(cdata->height>>1)<<1;
      bbot=xheight-1;
    }

    if (cdata->frame_width > cdata->width && rowstrides[0] < cdata->frame_width*psize) {
      // host ignores horizontal border
      bleft=bright=0;
    }
  }

#endif


  priv->current_pos=priv->input_position;

  max_frame_diff=2<<(priv->vstream->stpriv->keyframe_granule_shift-2);

  tframe+=priv->kframe_offset;

  if (tframe==priv->last_frame) {
#ifdef HAVE_THEORA
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
      theora_priv_t *tpriv=priv->tpriv;
      theora_decode_YUVout(&tpriv->ts, &yuv);
    }
#endif
#ifdef HAVE_DIRAC
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
      dirac_priv_t *dpriv=priv->dpriv;
      if (dpriv->schroframe!=NULL) {
        schroframe_to_pixel_data(dpriv->schroframe,(uint8_t **)pixel_data);
        return TRUE;
      }
      return FALSE;
    }
#endif
  } else {
#ifdef HAVE_DIRAC
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC&&dpriv->schroframe!=NULL) {
      schro_frame_unref(dpriv->schroframe);
      dpriv->schroframe=NULL;

      ppos_lower=priv->vstream->data_start;
      ppos_upper=opriv->total_bytes;

      max_frame_diff=16;
    }
#endif

    if (tframe<=priv->last_frame||tframe-priv->last_frame>max_frame_diff) {

      // this is a big kludge, because really for dirac we should always seek from the first frame
      // because frames can refer to any earlier frame
      if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
        tframe-=DIRAC_EXTRA_FRAMES ;
      }

      // need to find a new kframe
      pthread_mutex_lock(&priv->idxc->mutex);
      fidx=get_bounds_for((lives_clip_data_t *)cdata,tframe,&ppos_lower,&ppos_upper);
      if (fidx==NULL) {
        //printf("pt a\n");
        int64_t last_ret_frame=priv->last_frame;
        pthread_mutex_unlock(&priv->idxc->mutex);
        granulepos=ogg_seek((lives_clip_data_t *)cdata,tframe,ppos_lower,ppos_upper,TRUE);
        pthread_mutex_lock(&priv->idxc->mutex);
        priv->last_frame=last_ret_frame;
        if (granulepos==-1) return FALSE; // should never happen...
      } else granulepos=fidx->value;
      pthread_mutex_unlock(&priv->idxc->mutex);
      //printf("pt a2\n");

      if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
        kframe = granulepos >> priv->vstream->stpriv->keyframe_granule_shift;
        //printf("kframe %ld tframe %ld\n",kframe,tframe);
      } else {
        kframe=granulepos;
        // part 2 of the dirac kludge
        tframe+=DIRAC_EXTRA_FRAMES;
      }

      if (kframe<priv->kframe_offset) kframe=priv->kframe_offset;
    } else kframe=priv->last_kframe;

    priv->ignore_packets=FALSE;

    //printf("cf %ld and %ld\n",tframe,priv->last_frame);

    if (tframe>priv->last_frame&&(tframe-priv->last_frame<=max_frame_diff||
                                  (kframe==priv->last_kframe&&priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA))) {
      // same keyframe as last time, or next frame; we can continue where we left off
      cont=TRUE;
      priv->skip=tframe-priv->last_frame-1;
      priv->input_position=priv->current_pos;
      //printf("CONTINUING %ld %ld %ld %ld\n",tframe,priv->last_frame,kframe,priv->last_kframe);
    } else {
      if (fidx==NULL||priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
        if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
          pthread_mutex_lock(&priv->idxc->mutex);
          get_bounds_for((lives_clip_data_t *)cdata,kframe-1,&ppos_lower,&ppos_upper);
          pthread_mutex_unlock(&priv->idxc->mutex);
          granulepos=ogg_seek((lives_clip_data_t *)cdata,kframe-1,ppos_lower,ppos_upper,FALSE);
          //fprintf(stderr,"starting from found gpos %ld\n",granulepos);

          xkframe=granulepos >> priv->vstream->stpriv->keyframe_granule_shift;


          pthread_mutex_lock(&priv->idxc->mutex);
          get_bounds_for((lives_clip_data_t *)cdata,xkframe-1,&ppos_lower,&ppos_upper);
          pthread_mutex_unlock(&priv->idxc->mutex);
          granulepos=ogg_seek((lives_clip_data_t *)cdata,xkframe-1,ppos_lower,ppos_upper,FALSE);
          //fprintf(stderr,"starting from found gpos %ld\n",granulepos);

          xkframe=granulepos >> priv->vstream->stpriv->keyframe_granule_shift;

          priv->cframe = xkframe + granulepos-(xkframe<<priv->vstream->stpriv->keyframe_granule_shift); // cframe will be the next frame we decode
          //printf("xkframe is %ld %ld\n",xkframe,priv->cframe);
        } else {
          priv->cframe=kframe;
          priv->input_position=priv->cpagepos;
          //printf("SEEK TO %ld\n",priv->cpagepos);
        }

        if (priv->input_position==priv->vstream->data_start) {
          priv->cframe=kframe=priv->kframe_offset;
        }

        priv->skip=tframe-priv->cframe;
        //printf("skip is %ld - %ld = %ld\n",tframe,priv->cframe,priv->skip);
      } else {
        // same keyframe as last time
        priv->input_position=fidx->pagepos;
        priv->cframe=kframe=fidx->value;
        priv->skip=tframe-priv->cframe+1;
      }

      if (priv->input_position>priv->vstream->data_start) {
        priv->ignore_packets=TRUE;
      }

    }

    //fprintf(stderr,"getting yuv at %ld\n",input_position);

    priv->last_frame=tframe;
    priv->last_kframe=kframe;

#ifdef HAVE_THEORA
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
      if (!ogg_data_process((lives_clip_data_t *)cdata,&yuv,cont)) return FALSE;
    }
#endif

#ifdef HAVE_DIRAC
    if (priv->vstream->stpriv->fourcc_priv==FOURCC_DIRAC) {
      schro_decoder_set_earliest_frame(dpriv->schrodec, tframe);
      priv->cframe=tframe;
      return ogg_data_process((lives_clip_data_t *)cdata,pixel_data,cont);
    }
#endif

  }

#ifdef HAVE_THEORA
  if (priv->vstream->stpriv->fourcc_priv==FOURCC_THEORA) {
    size_t srcadd;

    for (p=0; p<nplanes; p++) {
      dst=pixel_data[p];

      switch (p) {
      case 0:
        src=yuv.y;
        srcadd=yuv.y_stride;
        break;
      case 1:
        src=yuv.u;
        srcadd=yuv.uv_stride;
        break;
      case 2:
        src=yuv.v;
        srcadd=yuv.uv_stride;
        break;
      default:
        srcadd=0;
        src=NULL;
        break;
      }

      for (i=0; i<xheight; i++) {
        if (i<btop||i>bbot) {
          // top or bottom border, copy black row
          if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||
              pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
            memset(dst,black[p],dstwidth+(bleft+bright)*psize);
            dst+=dstwidth+(bleft+bright)*psize;
          } else dst+=write_black_pixel(dst,pal,dstwidth/psize+bleft+bright,y_black);
          continue;
        }

        if (bleft>0) {
          if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
              pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
            memset(dst,black[p],bleft*psize);
            dst+=bleft*psize;
          } else dst+=write_black_pixel(dst,pal,bleft,y_black);
        }

        memcpy(dst,src,srcadd);
        dst+=dstwidth;

        if (bright>0) {
          if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||
              pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) {
            memset(dst,black[p],bright*psize);
            dst+=bright*psize;
          } else dst+=write_black_pixel(dst,pal,bright,y_black);
        }

        src+=srcadd;
      }
      if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P)) {
        dstwidth>>=1;
        bleft>>=1;
        bright>>=1;
      }
      if (p==0&&(pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P)) {
        xheight>>=1;
        btop>>=1;
        bbot>>=1;
      }
    }
    return TRUE;
  }
#endif

  return FALSE;
}




void clip_data_free(lives_clip_data_t *cdata) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  if (cdata->palettes!=NULL) free(cdata->palettes);
  cdata->palettes=NULL;

  // free index entries
  if (priv->idxc!=NULL) idxc_release(cdata);
  priv->idxc=NULL;

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  if (priv->opriv!=NULL) free(priv->opriv);

  if (priv!=NULL) free(priv);

  free(cdata);
}


void module_unload(void) {
  idxc_release_all();
}
