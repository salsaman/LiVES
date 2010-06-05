// LiVES - ogg/theora/vorbis decoder plugin
// (c) G. Finch 2008 - 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


// start decoding at the page whose granulepos converts to the highest frame number prior to the one you're looking for


#include "decplugin.h"

// palettes, etc.
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-effects.h"



///////////////////////////////////////////////////////
#include <inttypes.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#include <ogg/ogg.h>
#include <theora/theora.h>

#include "ogg_theora_decoder.h"



static int ogg_data_process(lives_clip_data_t *cdata, yuv_buffer *yuv, boolean cont);

static const char *plugin_version="LiVES ogg/theora decoder version 1.1";



////////////////////////////////////////////

// index_entries



static void index_entries_free (index_entry *idx) {
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
  return ie;
}



static index_entry *theora_index_entry_add (lives_clip_data_t *cdata, int64_t granule, int64_t pagepos) {
  // add or update entry for keyframe
  index_entry *idx,*oidx,*last_idx=NULL;
  int64_t gpos,frame,kframe,tframe,tkframe;
  
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  if (priv->vstream==NULL) return NULL;

  tkframe = granule >> priv->vstream->stpriv->keyframe_granule_shift;
  tframe = tkframe + granule-(tkframe<<priv->vstream->stpriv->keyframe_granule_shift);

  if (tkframe<1) return NULL;

  oidx=idx=priv->idx;

  if (idx==NULL) {
    index_entry *ie=index_entry_new();
    ie->granulepos=granule;
    ie->pagepos=pagepos;
    priv->idx=ie;
    return ie;
  }

  while (idx!=NULL) {
    gpos=idx->granulepos;

    kframe = gpos >> priv->vstream->stpriv->keyframe_granule_shift;
    if (kframe>tframe) break;

    if (kframe==tkframe) {
      // entry exists, update it if applicable, and return it in found
      frame = kframe + gpos-(kframe<<priv->vstream->stpriv->keyframe_granule_shift);
      if (frame<tframe) {
	idx->granulepos=granule;
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
  }
  else {
    idx->next=oidx;
    oidx=idx;
  }

  if (idx->next!=NULL) {
    idx->next->prev=idx;
  }

  idx->granulepos=granule;
  idx->pagepos=pagepos;

  return idx;
}



static index_entry *get_bounds_for (lives_clip_data_t *cdata, int64_t tframe, int64_t *ppos_lower, int64_t *ppos_upper) {
  // find upper and lower pagepos for frame; alternately if found!=NULL and we find an exact match, we return it
  int64_t kframe,frame,gpos;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  index_entry *idx=priv->idx;

  if (ppos_lower!=NULL) *ppos_lower=-1;
  if (ppos_upper!=NULL) *ppos_upper=-1;

  while (idx!=NULL) {

    if (idx->pagepos<0) {
      // kframe was found to be invalid
      idx=idx->next;
      continue;
    }

    gpos=idx->granulepos;
    kframe = gpos >> priv->vstream->stpriv->keyframe_granule_shift;

    //fprintf(stderr,"check %lld against %lld\n",tframe,kframe);

    if (kframe>tframe) {
      if (ppos_upper!=NULL) *ppos_upper=idx->pagepos+BYTES_TO_READ;
      return NULL;
    }

    frame = kframe + gpos-(kframe<<priv->vstream->stpriv->keyframe_granule_shift);
    //fprintf(stderr,"2check against %lld\n",frame);
    if (frame<tframe) {
      if (ppos_lower!=NULL) *ppos_lower=idx->pagepos;
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


static uint8_t * ptr_2_op(uint8_t * ptr, ogg_packet * op) {
  memcpy(op, ptr, sizeof(*op));
  ptr += sizeof(*op);
  op->packet = ptr;
  ptr += op->bytes;
  return ptr;
}


static int64_t get_page(lives_clip_data_t *cdata, int64_t inpos) {
  uint8_t header[PAGE_HEADER_BYTES+255];
  int nsegs, i;
  int64_t result, gpos;
  int page_size;
  char * buf;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  if (opriv->page_valid) {
    fprintf(stderr,"page valid !\n");
    return 0;
  }

  lseek64 (opriv->fd, inpos, SEEK_SET);

  if (read (opriv->fd, header, PAGE_HEADER_BYTES) < PAGE_HEADER_BYTES) {
    lseek64 (opriv->fd, inpos, SEEK_SET);
    return 0;
  }

  nsegs = header[PAGE_HEADER_BYTES-1];
  
  if (read (opriv->fd, header+PAGE_HEADER_BYTES, nsegs) < nsegs) {
    lseek64 (opriv->fd, inpos, SEEK_SET);
    return 0;
  }

  page_size = PAGE_HEADER_BYTES + nsegs;

  for (i=0;i<nsegs;i++) page_size += header[PAGE_HEADER_BYTES+i];

  ogg_sync_reset(&opriv->oy);

  buf = ogg_sync_buffer(&(opriv->oy), page_size);

  memcpy(buf,header,PAGE_HEADER_BYTES+nsegs);

  result = read (opriv->fd, (uint8_t*)buf+PAGE_HEADER_BYTES+nsegs, page_size-PAGE_HEADER_BYTES-nsegs);

  ogg_sync_wrote(&(opriv->oy), result+PAGE_HEADER_BYTES+nsegs);




  if (ogg_sync_pageout(&(opriv->oy), &(opriv->current_page)) != 1) {
    //fprintf(stderr, "Got no packet %lld %d %s\n",result,page_size,buf);
    return 0;
  }

  //fprintf(stderr, "Got packet %lld %d %s\n",result,page_size,buf);


  if (priv->vstream!=NULL) {
    if (ogg_page_serialno(&(opriv->current_page))==priv->vstream->stream_id) {
      gpos=ogg_page_granulepos(&(opriv->current_page));
      theora_index_entry_add (cdata, gpos, inpos);
    }
  }

  opriv->page_valid = 1;
  return result+PAGE_HEADER_BYTES+nsegs;
}



static uint32_t detect_stream(ogg_packet * op) {
  //fprintf(stderr,"detecting stream\n");
  if((op->bytes > 7) &&
     (op->packet[0] == 0x01) &&
     !strncmp((char*)(op->packet+1), "vorbis", 6)) {
    //fprintf(stderr,"vorbis found\n");
    return FOURCC_VORBIS;
  }
  else if((op->bytes > 7) &&
	  (op->packet[0] == 0x80) &&
	  !strncmp((char*)(op->packet+1), "theora", 6)) {
    //fprintf(stderr,"theora found\n");
    return FOURCC_THEORA;
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


static lives_in_stream *lives_in_stream_new (int type) {
  lives_in_stream *lstream=(lives_in_stream *)malloc(sizeof(lives_in_stream));
  lstream->type=type;
  lstream->ext_data=NULL;
  lstream->ext_size=0;
  return lstream;
}


static void append_extradata(lives_in_stream *s, ogg_packet * op) {
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


static int setup_tracks(lives_clip_data_t *cdata) {
  // pull first audio and video streams

  int done;
  stream_priv_t * ogg_stream;
  int serialno;
  int header_bytes = 0;
  int64_t input_pos;

  uint8_t imajor,iminor,isubminor;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  opriv->page_valid=0;

  lseek64(opriv->fd, priv->data_start, SEEK_SET);
  priv->input_position=priv->data_start;

  /* Get the first page of each stream */
  while(1) {
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
      return 0;
    }
      
    ogg_stream->fourcc_priv = detect_stream(&opriv->op);

    switch(ogg_stream->fourcc_priv) {
    case FOURCC_VORBIS:
      if (priv->astream!=NULL) {
	fprintf(stderr,"got extra audio stream\n");
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
    case FOURCC_THEORA:
      if (priv->vstream!=NULL) {
	fprintf(stderr,"got extra video stream\n");
	break;
      }
      priv->vstream = lives_in_stream_new(LIVES_STREAM_VIDEO);
      priv->vstream->fourcc = FOURCC_THEORA;
      priv->vstream->stpriv   = ogg_stream;
      priv->vstream->stream_id = serialno;

      ogg_stream->header_packets_needed = 3;
      append_extradata(priv->vstream, &opriv->op);
      ogg_stream->header_packets_read = 1;
      
      /* get version */
      imajor=((uint8_t *)(priv->vstream->ext_data))[55];
      iminor=((uint8_t *)(priv->vstream->ext_data))[56];
      isubminor=((uint8_t *)(priv->vstream->ext_data))[57];

      priv->vstream->version=imajor*1000000+iminor*1000+isubminor;

      // TODO - get frame width, height, picture width, height, and x and y offsets



      /* Get fps and keyframe shift */
      priv->vstream->fps_num = PTR_2_32BE(opriv->op.packet+22);
      priv->vstream->fps_denom = PTR_2_32BE(opriv->op.packet+26);
      
      //fprintf(stderr,"fps is %d / %d\n",priv->vstream->fps_num,priv->vstream->fps_denom);

      ogg_stream->keyframe_granule_shift = (char) ((opriv->op.packet[40] & 0x03) << 3);
      ogg_stream->keyframe_granule_shift |= (opriv->op.packet[41] & 0xe0) >> 5;

      /* Read remaining header packets from this page */
      while (ogg_stream_packetout(&ogg_stream->os, &opriv->op) == 1) {
	append_extradata(priv->vstream, &opriv->op);
	ogg_stream->header_packets_read++;
	if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed) break;
      }
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
      ogg_stream = (stream_priv_t*)(stream->stpriv);
      ogg_stream_pagein(&(ogg_stream->os), &(opriv->current_page));
      opriv->page_valid = 0;
      header_bytes += opriv->current_page.header_len + opriv->current_page.body_len;
      
      switch(ogg_stream->fourcc_priv) {
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
    }
    else {
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
   
  if (priv->data_start==0) priv->data_start=priv->input_position;

  return 1;
}



static void seek_byte(lives_clip_data_t *cdata, int64_t pos) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  ogg_sync_reset(&(opriv->oy));
  lseek64(opriv->fd,pos,SEEK_SET);
  priv->input_position=pos;
  opriv->page_valid=0;
}


/* Get new data */

static int64_t get_data(lives_clip_data_t *cdata) {
  int bytes_to_read;
  char * buf;
  int64_t result;
  
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  bytes_to_read = BYTES_TO_READ;

  if (opriv->total_bytes > 0) {
    if (priv->input_position + bytes_to_read > opriv->total_bytes)
      bytes_to_read = opriv->total_bytes - priv->input_position;
    if (bytes_to_read <= 0)
      return 0;
  }

  ogg_sync_reset(&opriv->oy);
  buf = ogg_sync_buffer(&(opriv->oy), bytes_to_read);

  lseek64 (opriv->fd, priv->input_position, SEEK_SET);
  result = read(opriv->fd, (uint8_t*)buf, bytes_to_read);

  opriv->page_valid=0;

  ogg_sync_wrote(&(opriv->oy), result);
  return result;
}


/* Find the first first page between pos1 and pos2,
   return file position, -1 is returned on failure */

static int64_t find_first_page(lives_clip_data_t *cdata, int64_t pos1, int64_t pos2, int serialno, int64_t *kframe, int64_t *frame) {
  long result;
  int64_t bytes;
  int64_t granulepos;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  seek_byte(cdata,pos1);

  if (pos1==priv->data_start) {
    // set a dummy granulepos at data_start
    if (frame) {
      *kframe=priv->kframe_offset;
      *frame=priv->kframe_offset;
    }
    opriv->page_valid=1;
    return priv->input_position;
  }

  while(1) {
    if (priv->input_position>=pos2) return -1;

    if (!(bytes=get_data(cdata))) return -1; // eof
    priv->input_position+=bytes;
    
    result = ogg_sync_pageseek(&opriv->oy, &opriv->current_page);

    if (!result) {
      // not found, need more data
      continue;
    }
    else if (result > 0) /* Page found, result is page size */
      {
	if (serialno!=ogg_page_serialno(&(opriv->current_page))||ogg_page_packets(&opriv->current_page)==0) {
	  // page is not for this stream, or no packet ends here
	  seek_byte(cdata,priv->input_position-bytes+result);
	  continue;
	}

	granulepos = ogg_page_granulepos(&(opriv->current_page));
	if (granulepos>0) theora_index_entry_add(cdata,granulepos,priv->input_position-bytes);

	if (frame) {
	  if (ogg_page_packets(&opriv->current_page)==0) {
	    // no frame ends on this page; but we need to find a frame number
	    seek_byte(cdata,priv->input_position-bytes+result);
	    continue;
	  }

	  *kframe =
	    granulepos >> priv->vstream->stpriv->keyframe_granule_shift;

	  *frame = *kframe +
	    granulepos-(*kframe<<priv->vstream->stpriv->keyframe_granule_shift);
    
	}

	opriv->page_valid=1;
	return priv->input_position-bytes;
      }
    else /* Skipped -result bytes */
      {
	seek_byte(cdata,priv->input_position-bytes-result);
	continue;
      }
  }
  return -1;
}




/* Find the last page between pos1 and pos2,
   return file position, -1 is returned on failure */

static int64_t find_last_page (lives_clip_data_t *cdata, int64_t pos1, int64_t pos2, int serialno, int64_t *kframe, int64_t *frame) {

  int64_t page_pos, last_page_pos = -1, start_pos;
  int64_t this_frame=0, last_frame = 0;
  int64_t this_kframe=0, last_kframe = 0;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  start_pos = pos2 - BYTES_TO_READ;

  while(1) {
    if (start_pos < priv->data_start) start_pos = priv->data_start;
    if (start_pos<pos1) start_pos=pos1;

    page_pos = find_first_page(cdata, start_pos, pos2, serialno, &this_kframe, &this_frame);

    if (page_pos == -1) {
      // no pages found in range
      if (last_page_pos >= 0)     /* No more pages in range -> return last one */
	{
	  if (frame) {
	    *frame = last_frame;
	    *kframe = last_kframe;
	  }
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
    }
    else {
      // found a page, see if we can find another one
      last_frame = this_frame;
      last_kframe = this_kframe;
      last_page_pos = page_pos;
      start_pos = page_pos+1;
    }
  }
  return -1;
}



static int64_t get_last_granulepos (lives_clip_data_t *cdata, int serialno) {
  // granulepos here is actually a frame - TODO ** fix for audio !

  int64_t pos, granulepos, kframe;
  lives_in_stream *stream;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;
  
  stream=stream_from_sno(cdata,serialno);
  if (stream==NULL) return -1;

  pos = find_last_page (cdata, priv->data_start, opriv->total_bytes, serialno, &kframe, &granulepos);
  if (pos < 0) return -1;

  return granulepos;

}



static double granulepos_2_time(lives_in_stream *s, int64_t pos) {
  int64_t frames;
  stream_priv_t *stream_priv = (stream_priv_t *)(s->stpriv);

  switch (stream_priv->fourcc_priv) {
  case FOURCC_VORBIS:
    //fprintf(stderr,"apos is %lld\n",pos);
    return ((long double)pos/(long double)s->samplerate);
    break;
  case FOURCC_THEORA:
    //fprintf(stderr,"vpos is %lld\n",pos);
    stream_priv = (stream_priv_t*)(s->stpriv);

    frames = pos;
    
    return ((double)s->fps_denom/(double)s->fps_num*(double)frames);
    break;
  }
  return -1;
}


static int stream_peek(int fd, char *str, size_t len) {
  off_t cpos=lseek(fd,0,SEEK_CUR); // get current posn 
  return pread(fd,str,len,cpos); // read len bytes without changing cpos
}




static int open_ogg(lives_clip_data_t *cdata) {
  double stream_duration;
  int64_t gpos;
  char scheck[4];

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  if (stream_peek(opriv->fd,scheck,4)<4) return 0;

  if (strncmp(scheck,"OggS",4)) return 0;

  ogg_sync_init(&(opriv->oy));

  priv->data_start=0;


  /* Set up the tracks */
  if (!setup_tracks(cdata)) {
    ogg_sync_clear(&opriv->oy);
    return 0;
  }

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
    //printf("priv->vstream duration is %.4f %lld\n",stream_duration,priv->vstream->nframes);
  }

  return 1;
}




static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle

  int i;
  uint8_t *ext_pos;
  ogg_packet op;
  struct stat sb;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  ogg_t *opriv=priv->opriv=(ogg_t *)malloc(sizeof(ogg_t));
  theora_priv_t *tpriv=priv->tpriv=(theora_priv_t *)malloc(sizeof(theora_priv_t));

  if ((opriv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "ogg_theora_decoder: unable to open %s\n",cdata->URI);
    free(opriv);
    priv->opriv=NULL;
    free(tpriv);
    priv->tpriv=NULL;
    return FALSE;
  }

  stat(cdata->URI,&sb);

  opriv->total_bytes=sb.st_size;

  opriv->page_valid=0;


  /* get ogg info */
  if (!open_ogg(cdata)) {
    close(opriv->fd);
    free(opriv);
    priv->opriv=NULL;
    free(tpriv);
    priv->tpriv=NULL;
    return FALSE;
  }



  // theora only


  // TODO - check all decoders, and init correct one

  /* Initialize theora structures */
  theora_info_init(&tpriv->ti);
  theora_comment_init(&tpriv->tc);
  
  /* Get header packets and initialize decoder */
  if (priv->vstream->ext_data==NULL) {
      fprintf(stderr, "Theora codec requires extra data");
      close(opriv->fd);
      free(opriv);
      priv->opriv=NULL;
      free(tpriv);
      priv->tpriv=NULL;
      return FALSE;
  }

  ext_pos = priv->vstream->ext_data;
  
  for (i=0;i<3;i++) {
    ext_pos = ptr_2_op(ext_pos, &op);
    theora_decode_header(&tpriv->ti, &tpriv->tc, &op);
  }
  
  theora_decode_init(&tpriv->ts, &tpriv->ti);
  

  priv->last_kframe=10000000;
  priv->last_frame=100000000;

  // index init 
  priv->idx=NULL;

  // check to find whether first frame is zero or one
  // if version >= 3.2.1 first kframe is 1; otherwise 0

  if (priv->vstream->version>=3002001) priv->kframe_offset=1;
  else priv->kframe_offset=0;

  return TRUE;
}




static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;
  theora_priv_t *tpriv=priv->tpriv;

  close(opriv->fd);

  ogg_sync_clear(&opriv->oy);


  // theora only
  // TODO - call function in decoder
  theora_clear(&tpriv->ts);
  theora_comment_clear(&tpriv->tc);
  theora_info_clear(&tpriv->ti);



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


  // free index entries
  if (priv->idx!=NULL) index_entries_free(priv->idx);
  priv->idx=NULL;

}


static inline int64_t frame_to_gpos(lives_clip_data_t *cdata, int64_t kframe, int64_t frame) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  return (kframe << priv->vstream->stpriv->keyframe_granule_shift) + (frame-kframe);
}


static int64_t ogg_seek(lives_clip_data_t *cdata, int64_t tframe, int64_t ppos_lower, int64_t ppos_upper, boolean can_exact) {
  // we do two passes here, first with can_exact set, then with can_exact unset

  // if can_exact is set, we find the granulepos nearest to or including the target granulepos
  // from this we extract an estimate of the keyframe (but note that there could be other keyframes between the found granulepos and the target)

  // on the second pass we find the highest granulepos < target. This places us just before or at the start of the target keyframe

  // when we come to decode, we start from this second position, discarding any completed packets on that page, 
  // and read pages discarding packets until we get to the target frame

  // we return the granulepos which we find


  int64_t start_pos,end_pos,pagepos,fpagepos,fframe,low_pagepos=-1,segsize,low_kframe=-1;
  int64_t frame,kframe,fkframe,low_frame=-1,high_frame=-1;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;

  //fprintf(stderr,"seek to frame %lld %d\n",tframe,can_exact);

  if (tframe<priv->kframe_offset) {
    if (!can_exact) {
      seek_byte(cdata,priv->data_start);
      return frame_to_gpos(cdata,priv->kframe_offset,1);
    }
    return frame_to_gpos(cdata,priv->kframe_offset,0); // TODO ** unless there is a real granulepos
  }

  if (ppos_lower<0) ppos_lower=priv->data_start;
  if (ppos_upper<0) ppos_upper=opriv->total_bytes;
  if (ppos_upper>opriv->total_bytes) ppos_upper=opriv->total_bytes;

  start_pos=ppos_lower;
  end_pos=ppos_upper;

  segsize=(end_pos-start_pos+1)>>1;
  start_pos+=segsize;

  while (1) {
    // see if the frame lies in current segment

    if (start_pos<ppos_lower) start_pos=ppos_lower;
    if (end_pos>ppos_upper) end_pos=ppos_upper;

    if (start_pos>=end_pos) {
      if (start_pos==ppos_lower) {
	if (!can_exact) seek_byte(cdata,start_pos);
	priv->cpagepos=start_pos;
	return frame_to_gpos(cdata,priv->kframe_offset,1);
      }
      // should never reach here
      fprintf(stderr,"oops\n");
      return -1;
    }

    //fprintf(stderr,"check seg %lld to %lld for %lld\n",start_pos,end_pos,tframe);

    if ((pagepos=find_first_page(cdata,start_pos,end_pos,priv->vstream->stream_id,&kframe,&frame))!=-1) {
      // found a page

      //fprintf(stderr,"first gp is %lld, kf %lld\n",frame,kframe);

      if (can_exact&&frame>=tframe&&kframe<=tframe) {
	// got it !
	//fprintf(stderr,"seek got keyframe %lld for %lld\n",kframe,tframe);
	priv->cpagepos=pagepos;
	return frame_to_gpos(cdata,kframe,frame);
      }

      if (frame>=tframe&&low_frame>-1&&low_frame<tframe) {
	// seek to nearest keyframe
	//fprintf(stderr,"set to low frame %lld,kf %lld\n",low_frame,low_kframe);
	if (!can_exact) seek_byte(cdata,low_pagepos);
	priv->cpagepos=low_pagepos;
	return frame_to_gpos(cdata,low_kframe,low_frame);
      }

      if (frame>=tframe) {
	// check previous segment
	low_frame=-1;
	high_frame=frame;
	start_pos-=segsize;
	end_pos-=segsize;
	continue;
      }

      fframe=frame;
      fpagepos=pagepos;
      fkframe=kframe;

      // see if it is in this segment
      //fprintf(stderr,"checking last page\n");

      if ((pagepos=find_last_page(cdata,start_pos-1,end_pos,priv->vstream->stream_id,&kframe,&frame))!=-1) {

	//fprintf(stderr,"last gp is %lld, kf %lld\n",frame,kframe);

	low_frame=frame;
	low_kframe=kframe;
	low_pagepos=pagepos;

	if (frame>=tframe&&kframe<=tframe&&can_exact) {
	  // got it !
	  //fprintf(stderr,"seek2 got keyframe %lld %lld\n",kframe,frame);
	  priv->cpagepos=pagepos;
	  return frame_to_gpos(cdata,kframe,frame);
	}

	if (frame<tframe&&high_frame!=-1&&high_frame>=tframe) {
	  //fprintf(stderr,"found high at %lld, using %lld\n",high_frame,frame);
	  if (!can_exact) seek_byte(cdata,pagepos);
	  priv->cpagepos=pagepos;
	  return frame_to_gpos(cdata,kframe,frame);
	}

	high_frame=-1;

	if (frame<tframe) {
	  // check next segment
	  start_pos+=segsize;
	  end_pos+=segsize;
	  continue;
	}

	// it is in this segment - check the first half, if we don't find it then we'll check the second half

	segsize=(segsize+1)>>1;
	if (segsize<BYTES_TO_READ) {
	  //fprintf(stderr,"seek3 got keyframe %lld for %lld\n",fkframe,tframe);
	  if (!can_exact) seek_byte(cdata,fpagepos);
	  priv->cpagepos=fpagepos;
	  return frame_to_gpos(cdata,fkframe,fframe);
	}

	end_pos-=segsize;
	continue;
      }
      break;
    }
    // we found no pages
    start_pos+=segsize;
    end_pos+=segsize;
    continue;
  }

  // should never reach here
  fprintf(stderr,"not found\n");
  return -1;
}



//////////////////////////////////////////
// std functions



const char *version(void) {
  return plugin_version;
}



lives_clip_data_t *init_cdata(void) {
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));
  lives_ogg_priv_t *priv;

  priv=cdata->priv=malloc(sizeof(lives_ogg_priv_t));

  priv->vstream=NULL;
  priv->astream=NULL;

  priv->opriv=NULL;
  priv->tpriv=NULL;

  priv->idx=NULL;

  cdata->palettes=malloc(2*sizeof(int));
  cdata->URI=NULL;

  return cdata;
}




lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI, a different current_clip, or a different current_palette, 
  // this must be called again with the same
  // cdata as the second parameter
  lives_ogg_priv_t *priv;
  ogg_t *opriv;
  theora_priv_t *tpriv;

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
    if (!attach_stream(cdata)) {
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

  opriv=priv->opriv;
  tpriv=priv->tpriv;

  cdata->nclips=1;

  // video part
  cdata->interlace=LIVES_INTERLACE_NONE;

  /* Get format */
  
  cdata->width  = opriv->y_width = tpriv->ti.frame_width;
  cdata->height = opriv->y_height = tpriv->ti.frame_height;
  
  cdata->fps  = (float)tpriv->ti.fps_numerator/(float)tpriv->ti.fps_denominator;
  
  switch(tpriv->ti.pixelformat) {
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
    return NULL;
  }
  
  cdata->palettes[1]=WEED_PALETTE_END;

  cdata->current_palette=cdata->palettes[0];

  cdata->nframes=priv->vstream->nframes;

  cdata->par=1.;

  // TODO
  cdata->offs_x=0;
  cdata->offs_y=0;
  cdata->frame_width=cdata->width;
  cdata->frame_height=cdata->height;

  cdata->YUV_clamping=WEED_YUV_CLAMPING_CLAMPED;
  cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
  cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT;

  sprintf(cdata->container_name,"%s","ogg");
  sprintf(cdata->video_name,"%s","theora");

  // audio part
  if (priv->astream!=NULL) {
    sprintf(cdata->audio_name,"%s","vorbis");
  }
  else memset(cdata->audio_name,0,1);
  
  return cdata;
}



static boolean ogg_theora_read(lives_clip_data_t *cdata, ogg_packet *op, yuv_buffer *yuv) {
  // this packet is for our theora decoder
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  theora_priv_t *tpriv=priv->tpriv;

  if (theora_decode_packetin(&tpriv->ts,op)==0) {
    if (priv->skip<=0) {
      if (theora_decode_YUVout(&tpriv->ts, yuv)==0) {
	priv->frame_out=TRUE;
      }
    }
  }
  return priv->frame_out;
}



static boolean ogg_data_process(lives_clip_data_t *cdata, yuv_buffer *yuv, boolean cont) {
  int64_t input_pos=0;
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;
  boolean ignore_count=(priv->ignore_packets&&!cont);


  priv->frame_out=FALSE;

  if (!cont) ogg_stream_reset(&priv->vstream->stpriv->os);

  while (!priv->frame_out) {
    opriv->page_valid=0;

    if (!cont) {
      if (!(input_pos=get_page(cdata,priv->input_position))) {
	// should never reach here
	fprintf(stderr, "EOF1 while decoding\n");
	return FALSE;
      }
      
      priv->input_position+=input_pos;
      
      if (ogg_page_serialno(&(opriv->current_page))!=priv->vstream->stream_id) continue;
      ogg_stream_pagein(&priv->vstream->stpriv->os, &(opriv->current_page));
    }

    while (ogg_stream_packetout(&priv->vstream->stpriv->os, &opriv->op) > 0) {
      if (yuv!=NULL) {
      // cframe is the frame we are about to decode

	//fprintf(stderr,"cframe is %d\n",cframe);

	if (priv->cframe==priv->last_kframe&&!ignore_count) {
	  priv->ignore_packets=FALSE; // reached kframe before target, start decoding packets
	}
	
	if (!priv->ignore_packets) {
	  ogg_theora_read(cdata,&opriv->op,yuv);
	}
      }

      if (!ignore_count) {
	priv->cframe++;
	priv->skip--;
      }
      if (yuv==NULL) priv->frame_out=TRUE;

      if (priv->frame_out) break;

      sched_yield();
    }

    ignore_count=FALSE;
    cont=FALSE;
  }

  return TRUE;
}


boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, void **pixel_data) {
  // seek to frame, and return pixel_data
  yuv_buffer yuv;
  boolean crow=FALSE;
  void *y,*u,*v;
  int64_t kframe=-1,xkframe;
  boolean cont=FALSE;
  int max_frame_diff;
  int64_t granulepos;
  static int64_t last_cframe;

  int64_t ppos_lower,ppos_upper;

  register int i;

  static index_entry *fidx=NULL;

  int mheight;

  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;
  ogg_t *opriv=priv->opriv;
  theora_priv_t *tpriv=priv->tpriv;

  priv->current_pos=priv->input_position;

  mheight=(opriv->y_height>>1)<<1; // yes indeed, there is a file with a height of 601 pixels...

  max_frame_diff=2<<(priv->vstream->stpriv->keyframe_granule_shift-2);

  tframe+=priv->kframe_offset;

  if (tframe==priv->last_frame) {
    theora_decode_YUVout(&tpriv->ts, &yuv);
  }
  else {
    if (tframe<=priv->last_frame||tframe-priv->last_frame>max_frame_diff) {
      // need to find a new kframe
      fidx=get_bounds_for((lives_clip_data_t *)cdata,tframe,&ppos_lower,&ppos_upper);
      if (fidx==NULL) {
	granulepos=ogg_seek((lives_clip_data_t *)cdata,tframe,ppos_lower,ppos_upper,TRUE);
	if (granulepos==-1) return FALSE; // should never happen...
	fidx=theora_index_entry_add((lives_clip_data_t *)cdata,granulepos,priv->cpagepos);
      }
      else granulepos=fidx->granulepos;
      kframe = granulepos >> priv->vstream->stpriv->keyframe_granule_shift;
      if (kframe<priv->kframe_offset) kframe=priv->kframe_offset;
    }
    else kframe=priv->last_kframe;
    
    priv->ignore_packets=FALSE;
    
    if (tframe>priv->last_frame&&((tframe-priv->last_frame<=max_frame_diff||kframe==priv->last_kframe))) {
      // same keyframe as last time, or next frame; we can continue where we left off
      cont=TRUE;
      priv->skip=tframe-priv->last_frame-1;
      priv->input_position=priv->current_pos;
    }
    else {
      if (fidx==NULL||fidx->prev==NULL) {
	get_bounds_for((lives_clip_data_t *)cdata,kframe-1,&ppos_lower,&ppos_upper);
	granulepos=ogg_seek((lives_clip_data_t *)cdata,kframe-1,ppos_lower,ppos_upper,FALSE);
	//fprintf(stderr,"starting from found gpos %lld\n",granulepos);
	xkframe=granulepos >> priv->vstream->stpriv->keyframe_granule_shift;
	priv->cframe = xkframe + granulepos-(xkframe<<priv->vstream->stpriv->keyframe_granule_shift)+1; // cframe will be the next frame we decode
	if (priv->input_position==priv->data_start) {
	  priv->cframe=kframe=1;
	}
	else {
	  theora_index_entry_add((lives_clip_data_t *)cdata,granulepos,priv->input_position);
	}
	last_cframe=priv->cframe;
	priv->skip=tframe-priv->cframe;
      }
      else {
	// same keyframe as last time, but we are reversing
	priv->input_position=fidx->prev->pagepos;
	granulepos=fidx->prev->granulepos;
	//fprintf(stderr,"starting from gpos %lld\n",granulepos);
	xkframe=granulepos >> priv->vstream->stpriv->keyframe_granule_shift;
	priv->cframe = xkframe + granulepos-(xkframe<<priv->vstream->stpriv->keyframe_granule_shift)+1; // cframe will be the next frame we decode
	priv->skip=tframe-priv->cframe;
      }
      
      if (priv->input_position>priv->data_start) {
	priv->ignore_packets=TRUE;
      }
      
    }
    
    //fprintf(stderr,"getting yuv at %lld\n",input_position);
    
    priv->last_frame=tframe;
    priv->last_kframe=kframe;
    if (!ogg_data_process((lives_clip_data_t *)cdata,&yuv,cont)) return FALSE;
  }

  y=yuv.y;
  u=yuv.u;
  v=yuv.v;

  for (i=0;i<mheight;i++) {
    memcpy(pixel_data[0],y,opriv->y_width);
    pixel_data[0]+=opriv->y_width;
    y+=yuv.y_stride;
    if (yuv.y_height==yuv.uv_height||crow) {
      memcpy(pixel_data[1],u,opriv->uv_width);
      memcpy(pixel_data[2],v,opriv->uv_width);
      pixel_data[1]+=opriv->uv_width;
      pixel_data[2]+=opriv->uv_width;
      u+=yuv.uv_stride;
      v+=yuv.uv_stride;
    }
    crow=!crow;
    sched_yield();
  }

  return TRUE;

}
  



void clip_data_free(lives_clip_data_t *cdata) {
  lives_ogg_priv_t *priv=(lives_ogg_priv_t *)cdata->priv;

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  if (priv->opriv!=NULL) free(priv->opriv);
  if (priv->tpriv!=NULL) free(priv->tpriv);
  
  if (priv!=NULL) free(priv);
  
  free(cdata->palettes);
  free(cdata);
}


