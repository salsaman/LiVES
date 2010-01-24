// LiVES - ogg/theora/vorbis decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl>
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

#include <ogg/ogg.h>
#include <theora/theora.h>

#include "ogg_theora_decoder.h"

static const char *plugin_version="LiVES ogg/theora decoder version 1.0";

static lives_clip_data_t cdata;
static ogg_t opriv;
static lives_in_stream *astream=NULL,*vstream=NULL;
static theora_priv_t tpriv;
static int64_t data_start;
static char *old_URI=NULL;

// seeking
static off_t input_position;
static int skip;
static int64_t last_kframe,last_frame,cframe,kframe_offset;
static int64_t cpagepos;
static boolean ignore_packets,frame_out;

// indexing
static index_entry *indexa;

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
  ie->next=NULL;
  ie->prev=NULL;
  return ie;
}



static index_entry *index_entry_add (index_entry *idx, int64_t granule, int64_t pagepos, index_entry **found) {
  // add or update entry for keyframe
  index_entry *oidx=idx,*last_idx=NULL;
  int64_t gpos,frame,kframe,tframe,tkframe;
  
  if (found!=NULL) *found=NULL;

  if (vstream==NULL) return NULL;

  tkframe = granule >> vstream->priv->keyframe_granule_shift;
  tframe = tkframe + granule-(tkframe<<vstream->priv->keyframe_granule_shift);

  if (tkframe<1) return idx;

  if (idx==NULL) {
    index_entry *ie=index_entry_new();
    ie->granulepos=granule;
    ie->pagepos=pagepos;
    if (found!=NULL) *found=ie;
    return ie;
  }


  while (idx!=NULL) {
    gpos=idx->granulepos;

    kframe = gpos >> vstream->priv->keyframe_granule_shift;
    if (kframe>tframe) break;

    if (kframe==tkframe) {
      // entry exists, update it if applicable, and return it in found
      frame = kframe + gpos-(kframe<<vstream->priv->keyframe_granule_shift);
      if (frame<tframe) {
	idx->granulepos=granule;
	idx->pagepos=pagepos;
      }
      if (found!=NULL) *found=idx;
      return oidx;
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

  if (found!=NULL) *found=idx;

  return oidx;
}



static index_entry *get_bounds_for (index_entry *idx, int64_t tframe, int64_t *ppos_lower, int64_t *ppos_upper) {
  // find upper and lower pagepos for frame; alternately if found!=NULL and we find an exact match, we return it
  int64_t kframe,frame,gpos;

  if (ppos_lower!=NULL) *ppos_lower=-1;
  if (ppos_upper!=NULL) *ppos_upper=-1;

  while (idx!=NULL) {

    if (idx->pagepos<0) {
      // kframe was found to be invalid
      idx=idx->next;
      continue;
    }

    gpos=idx->granulepos;
    kframe = gpos >> vstream->priv->keyframe_granule_shift;

    //fprintf(stderr,"check %lld against %lld\n",tframe,kframe);

    if (kframe>tframe) {
      if (ppos_upper!=NULL) *ppos_upper=idx->pagepos+BYTES_TO_READ;
      return NULL;
    }

    frame = kframe + gpos-(kframe<<vstream->priv->keyframe_granule_shift);
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


static int64_t get_page(int64_t inpos) {
  uint8_t header[PAGE_HEADER_BYTES+255];
  int nsegs, i;
  int64_t result, gpos;
  int page_size;
  char * buf;

  if (opriv.page_valid) {
    fprintf(stderr,"page valid !\n");
    return 0;
  }

  lseek (opriv.fd, inpos, SEEK_SET);

  if (read (opriv.fd, header, PAGE_HEADER_BYTES) < PAGE_HEADER_BYTES) {
    lseek (opriv.fd, inpos, SEEK_SET);
    return 0;
  }

  nsegs = header[PAGE_HEADER_BYTES-1];
  
  if (read (opriv.fd, header+PAGE_HEADER_BYTES, nsegs) < nsegs) {
    lseek (opriv.fd, inpos, SEEK_SET);
    return 0;
  }

  page_size = PAGE_HEADER_BYTES + nsegs;

  for (i=0;i<nsegs;i++) page_size += header[PAGE_HEADER_BYTES+i];

  ogg_sync_reset(&opriv.oy);

  buf = ogg_sync_buffer(&(opriv.oy), page_size);

  opriv.current_page_pos = inpos;

  memcpy(buf,header,PAGE_HEADER_BYTES+nsegs);

  result = read (opriv.fd, (uint8_t*)buf+PAGE_HEADER_BYTES+nsegs, page_size-PAGE_HEADER_BYTES-nsegs);

  ogg_sync_wrote(&(opriv.oy), result+PAGE_HEADER_BYTES+nsegs);

  if (ogg_sync_pageout(&(opriv.oy), &(opriv.current_page)) != 1) {
    //fprintf(stderr, "Got no packet %lld %d %s\n",result,page_size,buf);
    return 0;
  }

  //fprintf(stderr, "Got packet %lld %d %s\n",result,page_size,buf);


  if (vstream!=NULL) {
    if (ogg_page_serialno(&(opriv.current_page))==vstream->stream_id) {
      gpos=ogg_page_granulepos(&(opriv.current_page));
      index_entry_add (indexa, gpos, inpos, NULL);
    }
  }

  opriv.page_valid = 1;
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


lives_in_stream *lives_in_stream_new (int type) {
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


static inline lives_in_stream *stream_from_sno(int sno) {
  if (astream!=NULL&&sno==astream->stream_id) return astream;
  if (vstream!=NULL&&sno==vstream->stream_id) return vstream;
  return NULL;
}


static int setup_track(int64_t start_position) {
  // pull first audio and video streams

  int done;
  stream_priv_t * ogg_stream;
  int serialno;
  int header_bytes = 0;
  int64_t input_pos;

  opriv.page_valid=0;

  lseek(opriv.fd, start_position, SEEK_SET);
  input_position=start_position;

  /* Get the first page of each stream */
  while(1) {
    if (!(input_pos=get_page(input_position))) {
      //fprintf(stderr, "EOF1 while setting up track\n");
      return 0;
    }

    input_position+=input_pos;

    if (!ogg_page_bos(&(opriv.current_page))) {
      opriv.page_valid = 1;
      break;
    }

    /* Setup stream */
    serialno = ogg_page_serialno(&(opriv.current_page));
    ogg_stream = calloc(1, sizeof(*ogg_stream));
    ogg_stream->last_granulepos = -1;
    
    ogg_stream_init(&ogg_stream->os, serialno);
    ogg_stream_pagein(&ogg_stream->os, &(opriv.current_page));
    opriv.page_valid = 0;
    header_bytes += opriv.current_page.header_len + opriv.current_page.body_len;

    if (ogg_stream_packetout(&ogg_stream->os, &opriv.op) != 1) {
      fprintf(stderr, "EOF3 while setting up track\n");
      return 0;
    }
      
    ogg_stream->fourcc_priv = detect_stream(&opriv.op);

    switch(ogg_stream->fourcc_priv) {
    case FOURCC_VORBIS:
      if (astream!=NULL) {
	fprintf(stderr,"got extra audio stream\n");
	break;
      }
      astream = lives_in_stream_new(LIVES_STREAM_AUDIO);
      astream->fourcc = FOURCC_VORBIS;
      astream->priv   = ogg_stream;
      astream->stream_id = serialno;

      ogg_stream->header_packets_needed = 3;
      append_extradata(astream, &opriv.op);
      ogg_stream->header_packets_read = 1;

      /* Get samplerate */
      astream->samplerate=PTR_2_32LE(opriv.op.packet + 12);
      //fprintf(stderr,"rate is %d\n",astream->samplerate);
      
      /* Read remaining header packets from this page */
      while (ogg_stream_packetout(&ogg_stream->os, &opriv.op) == 1) {
	append_extradata(astream, &opriv.op);
	ogg_stream->header_packets_read++;
	if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed)  break;
      }
      break;
    case FOURCC_THEORA:
      if (vstream!=NULL) {
	fprintf(stderr,"got extra video stream\n");
	break;
      }
      vstream = lives_in_stream_new(LIVES_STREAM_VIDEO);
      vstream->fourcc = FOURCC_THEORA;
      vstream->priv   = ogg_stream;
      vstream->stream_id = serialno;

      ogg_stream->header_packets_needed = 3;
      append_extradata(vstream, &opriv.op);
      ogg_stream->header_packets_read = 1;
      
      /* Get fps and keyframe shift */
      vstream->fps_num = PTR_2_32BE(opriv.op.packet+22);
      vstream->fps_denom = PTR_2_32BE(opriv.op.packet+26);
      
      //fprintf(stderr,"fps is %d / %d\n",vstream->fps_num,vstream->fps_denom);

      ogg_stream->keyframe_granule_shift = (char) ((opriv.op.packet[40] & 0x03) << 3);
      ogg_stream->keyframe_granule_shift |= (opriv.op.packet[41] & 0xe0) >> 5;

      /* Read remaining header packets from this page */
      while (ogg_stream_packetout(&ogg_stream->os, &opriv.op) == 1) {
	append_extradata(vstream, &opriv.op);
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

  if (vstream==NULL) return 0;


  /*
   *  Now, read header pages until we are done, current_page still contains the
   *  first page which has no bos marker set
   */

  done = 0;
  while (!done) {
    lives_in_stream *stream = stream_from_sno(ogg_page_serialno(&(opriv.current_page)));
    if (stream) {
      ogg_stream = (stream_priv_t*)(stream->priv);
      ogg_stream_pagein(&(ogg_stream->os), &(opriv.current_page));
      opriv.page_valid = 0;
      header_bytes += opriv.current_page.header_len + opriv.current_page.body_len;
      
      switch(ogg_stream->fourcc_priv) {
      case FOURCC_THEORA:
      case FOURCC_VORBIS:
	/* Read remaining header packets from this page */
	while (ogg_stream_packetout(&ogg_stream->os, &opriv.op) == 1) {
	  append_extradata(stream, &opriv.op);
	  ogg_stream->header_packets_read++;
	  /* Second packet is vorbis comment starting after 7 bytes */
	  //if (ogg_stream->header_packets_read == 2) parse_vorbis_comment(stream, opriv.op.packet + 7, opriv.op.bytes - 7);
	  if (ogg_stream->header_packets_read == ogg_stream->header_packets_needed) break;
	}
	break;
      }
    }
    else {
      opriv.page_valid = 0;
      header_bytes += opriv.current_page.header_len + opriv.current_page.body_len;
    }
    
    /* Check if we are done for all streams */
    done = 1;
    
    //for (i = 0; i < track->num_audio_streams; i++) {
    if (astream!=NULL) {
      ogg_stream = astream->priv;
      if (ogg_stream->header_packets_read < ogg_stream->header_packets_needed) done = 0;
    }
    
    if (done) {
      //for(i = 0; i < track->num_video_streams; i++)
      //{
      if (vstream!=NULL) {
	ogg_stream = vstream->priv;
	if (ogg_stream->header_packets_read < ogg_stream->header_packets_needed) done = 0;
      }
    }
    //}
    
    /* Read the next page if we aren't done yet */
    
    if (!done) {
      if (!(input_pos=get_page(input_position))) {
	fprintf(stderr, "EOF2 while setting up track");
	return 0;
      }
      input_position+=input_pos;
    }
  }
   
  if (data_start==0) data_start=input_position;

  return 1;
}



static void seek_byte(int64_t pos) {
  ogg_sync_reset(&(opriv.oy));
  lseek(opriv.fd,pos,SEEK_SET);
  input_position=pos;
  opriv.page_valid=0;
}


/* Get new data */

static int64_t get_data(void) {
  int bytes_to_read;
  char * buf;
  int result;
  
  bytes_to_read = BYTES_TO_READ;

  if (opriv.total_bytes > 0) {
    if (input_position + bytes_to_read > opriv.total_bytes)
      bytes_to_read = opriv.total_bytes - input_position;
    if (bytes_to_read <= 0)
      return 0;
  }

  ogg_sync_reset(&opriv.oy);
  buf = ogg_sync_buffer(&(opriv.oy), bytes_to_read);

  lseek (opriv.fd, input_position, SEEK_SET);
  result = read(opriv.fd, (uint8_t*)buf, bytes_to_read);

  opriv.page_valid=0;

  ogg_sync_wrote(&(opriv.oy), result);
  return result;
}


/* Find the first first page between pos1 and pos2,
   return file position, -1 is returned on failure */

static int64_t find_first_page(int64_t pos1, int64_t pos2, int serialno, int64_t *kframe, int64_t *frame) {
  long result;
  int64_t bytes;
  int64_t granulepos;

  seek_byte(pos1);

  if (pos1==data_start) {
    // set a dummy granulepos at data_start
    if (frame) {
      *kframe=kframe_offset;
      *frame=kframe_offset;
    }
    opriv.page_valid=1;
    return input_position;
  }

  while(1) {
    if (input_position>=pos2) return -1;

    if (!(bytes=get_data())) return -1; // eof
    input_position+=bytes;
    
    result = ogg_sync_pageseek(&opriv.oy, &opriv.current_page);

    if (!result) {
      // not found, need more data
      continue;
    }
    else if (result > 0) /* Page found, result is page size */
      {
	if (serialno!=ogg_page_serialno(&(opriv.current_page))||ogg_page_packets(&opriv.current_page)==0) {
	  // page is not for this stream, or no packet ends here
	  seek_byte(input_position-bytes+result);
	  continue;
	}

	granulepos = ogg_page_granulepos(&(opriv.current_page));
	if (granulepos>0) index_entry_add(indexa,granulepos,input_position-bytes,NULL);

	if (frame) {
	  if (ogg_page_packets(&opriv.current_page)==0) {
	    // no frame ends on this page; but we need to find a frame number
	    seek_byte(input_position-bytes+result);
	    continue;
	  }

	  *kframe =
	    granulepos >> vstream->priv->keyframe_granule_shift;

	  *frame = *kframe +
	    granulepos-(*kframe<<vstream->priv->keyframe_granule_shift);
    
	}

	opriv.page_valid=1;
	return input_position-bytes;
      }
    else /* Skipped -result bytes */
      {
	seek_byte(input_position-bytes-result);
	continue;
      }
  }
  return -1;
}




/* Find the last page between pos1 and pos2,
   return file position, -1 is returned on failure */

static int64_t find_last_page (int64_t pos1, int64_t pos2, int serialno, int64_t *kframe, int64_t *frame) {

  int64_t page_pos, last_page_pos = -1, start_pos;
  int64_t this_frame=0, last_frame = 0;
  int64_t this_kframe=0, last_kframe = 0;

  start_pos = pos2 - BYTES_TO_READ;

  while(1) {
    if (start_pos < data_start) start_pos = data_start;
    if (start_pos<pos1) start_pos=pos1;

    page_pos = find_first_page(start_pos, pos2, serialno, &this_kframe, &this_frame);

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



static int64_t get_last_granulepos (int serialno) {
  // granulepos here is actually a frame - TODO ** fix for audio !

  int64_t pos, granulepos, kframe;
  lives_in_stream *stream;
  
  stream=stream_from_sno(serialno);
  if (stream==NULL) return -1;
  pos = find_last_page (data_start, opriv.total_bytes, serialno, &kframe, &granulepos);
  if (pos < 0) return -1;

  return granulepos;

}



static double granulepos_2_time(lives_in_stream *s, int64_t pos) {
  int64_t frames;
  stream_priv_t *stream_priv = (stream_priv_t *)(s->priv);

  switch (stream_priv->fourcc_priv) {
  case FOURCC_VORBIS:
    //fprintf(stderr,"apos is %lld\n",pos);
    return ((long double)pos/(long double)s->samplerate);
    break;
  case FOURCC_THEORA:
    //fprintf(stderr,"vpos is %lld\n",pos);
    stream_priv = (stream_priv_t*)(s->priv);

    frames = pos;
    
    return ((double)s->fps_denom/(double)s->fps_num*(double)frames);
    break;
  }
  return -1;
}



static int open_ogg(void) {
  double stream_duration;
  int64_t gpos;

  ogg_sync_init(&(opriv.oy));

  data_start=0;

  /* Set up the first track */
  if (!setup_track(data_start)) {
    ogg_sync_clear(&opriv.oy);
    return 0;
  }

  if (astream!=NULL) {
    ogg_stream_reset(&astream->priv->os);
    gpos=get_last_granulepos(astream->stream_id);
    stream_duration =
      granulepos_2_time(astream,gpos);
    astream->duration=stream_duration;
    astream->priv->last_granulepos=gpos;
    //printf("astream duration is %.4f\n",stream_duration);
  }

  if (vstream!=NULL) {
    ogg_stream_reset(&vstream->priv->os);
    gpos=get_last_granulepos(vstream->stream_id);
    
    /*  kframe=gpos >> vstream->priv->keyframe_granule_shift;
	vstream->nframes = kframe + gpos-(kframe<<vstream->priv->keyframe_granule_shift);*/

    vstream->nframes=gpos-kframe_offset;

    stream_duration =
      granulepos_2_time(vstream,gpos);
    vstream->duration=stream_duration;
    vstream->priv->last_granulepos=gpos;
    //printf("vstream duration is %.4f %lld\n",stream_duration,vstream->nframes);
  }

  return 1;
}




static boolean attach_stream(char *URI) {
  // open the file and get a handle

  int i;
  uint8_t *ext_pos;
  ogg_packet op;
  struct stat sb;

  int64_t granulepos,testkframe;

  if (!(opriv.fd=open(URI,O_RDONLY))) {
    fprintf(stderr, "ogg_theora_decoder: unable to open %s\n",URI);
    return FALSE;
  }

  stat(URI,&sb);

  opriv.total_bytes=sb.st_size;

  opriv.page_valid=0;

  /* get ogg info */
  if (!open_ogg()) {
    close(opriv.fd);
    return FALSE;
  }

  /* Initialize theora structures */
  theora_info_init(&tpriv.ti);
  theora_comment_init(&tpriv.tc);
  
  /* Get header packets and initialize decoder */
  if (vstream->ext_data==NULL) {
      fprintf(stderr, "Theora codec requires extra data");
      return FALSE;
  }

  ext_pos = vstream->ext_data;
  
  for (i=0;i<3;i++) {
    ext_pos = ptr_2_op(ext_pos, &op);
    theora_decode_header(&tpriv.ti, &tpriv.tc, &op);
  }
  
  theora_decode_init(&tpriv.ts, &tpriv.ti);
  
  last_kframe=10000000;
  last_frame=100000000;

  indexa=NULL;


  // check to find whether first frame is zero or one

  seek_byte(data_start);
  ignore_packets=FALSE;
  cframe=0;

  // process until we get frame(s)
  ogg_data_process(NULL,FALSE);

  granulepos=ogg_page_granulepos(&opriv.current_page);
  testkframe=granulepos >> vstream->priv->keyframe_granule_shift;

  // check testframe against cframe
  //fprintf(stderr,"first kframe is %lld\n",testkframe);

  kframe_offset=testkframe;
  
  return TRUE;
}




static void detach_stream (char *URI) {
  // close the file, free the decoder
  close(opriv.fd);

  ogg_sync_clear(&opriv.oy);

  theora_clear(&tpriv.ts);
  theora_comment_clear(&tpriv.tc);
  theora_info_clear(&tpriv.ti);

  if (astream!=NULL) {
    if (astream->ext_data!=NULL) free(astream->ext_data);
    ogg_stream_clear(&astream->priv->os);
    free(astream->priv);
    free(astream);
    astream=NULL;
  }

  if (vstream!=NULL) {
    if (vstream->ext_data!=NULL) free(vstream->ext_data);
    ogg_stream_clear(&vstream->priv->os);
    free(vstream->priv);
    free(vstream);
    vstream=NULL;
  }

  if (indexa!=NULL) index_entries_free(indexa);
  indexa=NULL;

}


static inline int64_t frame_to_gpos(int64_t kframe, int64_t frame) {
  return (kframe << vstream->priv->keyframe_granule_shift) + (frame-kframe);
}


static int64_t ogg_seek(int64_t tframe, int64_t ppos_lower, int64_t ppos_upper, boolean can_exact) {
  // we do two passes here, first with can_exact set, then with can_exact unset

  // if can_exact is set, we find the granulepos nearest to or including the target granulepos
  // from this we extract an estimate of the keyframe (but note that there could be other keyframes between the found granulepos and the target)

  // on the second pass we find the highest granulepos < target. This places us just before or at the start of the target keyframe

  // when we come to decode, we start from this second position, discarding any completed packets on that page, 
  // and read pages discarding packets until we get to the target frame

  // we return the granulepos which we find


  int64_t start_pos,end_pos,pagepos,fpagepos,fframe,low_pagepos=-1,segsize,low_kframe=-1;
  int64_t frame,kframe,fkframe,low_frame=-1,high_frame=-1;

  //fprintf(stderr,"seek to frame %lld %d\n",tframe,can_exact);

  if (tframe<kframe_offset) {
    if (!can_exact) {
      seek_byte(data_start);
      return frame_to_gpos(kframe_offset,1);
    }
    return frame_to_gpos(1,0); // TODO ** unless there is a real granulepos
  }

  if (ppos_lower<0) ppos_lower=data_start;
  if (ppos_upper<0) ppos_upper=opriv.total_bytes;
  if (ppos_upper>opriv.total_bytes) ppos_upper=opriv.total_bytes;

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
	if (!can_exact) seek_byte(start_pos);
	cpagepos=start_pos;
	return frame_to_gpos(1,1);
      }
      // should never reach here
      fprintf(stderr,"oops\n");
      return -1;
    }

    //fprintf(stderr,"check seg %lld to %lld for %lld\n",start_pos,end_pos,tframe);

    if ((pagepos=find_first_page(start_pos,end_pos,vstream->stream_id,&kframe,&frame))!=-1) {
      // found a page

      //fprintf(stderr,"first gp is %lld, kf %lld\n",frame,kframe);

      if (can_exact&&frame>=tframe&&kframe<=tframe) {
	// got it !
	//fprintf(stderr,"seek got keyframe %lld for %lld\n",kframe,tframe);
	cpagepos=pagepos;
	return frame_to_gpos(kframe,frame);
      }

      if (frame>=tframe&&low_frame>-1&&low_frame<tframe) {
	// seek to nearest keyframe
	//fprintf(stderr,"set to low frame %lld,kf %lld\n",low_frame,low_kframe);
	if (!can_exact) seek_byte(low_pagepos);
	cpagepos=low_pagepos;
	return frame_to_gpos(low_kframe,low_frame);
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

      if ((pagepos=find_last_page(start_pos-1,end_pos,vstream->stream_id,&kframe,&frame))!=-1) {

	//fprintf(stderr,"last gp is %lld, kf %lld\n",frame,kframe);

	low_frame=frame;
	low_kframe=kframe;
	low_pagepos=pagepos;

	if (frame>=tframe&&kframe<=tframe&&can_exact) {
	  // got it !
	  //fprintf(stderr,"seek2 got keyframe %lld %lld\n",kframe,frame);
	  cpagepos=pagepos;
	  return frame_to_gpos(kframe,frame);
	}

	if (frame<tframe&&high_frame!=-1&&high_frame>=tframe) {
	  //fprintf(stderr,"found high at %lld, using %lld\n",high_frame,frame);
	  if (!can_exact) seek_byte(pagepos);
	  cpagepos=pagepos;
	  return frame_to_gpos(kframe,frame);
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
	  if (!can_exact) seek_byte(fpagepos);
	  cpagepos=fpagepos;
	  return frame_to_gpos(fkframe,fframe);
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



const char *module_check_init(void) {
  cdata.palettes=malloc(2*sizeof(int));
  return NULL;
}


const char *version(void) {
  return plugin_version;
}



const lives_clip_data_t *get_clip_data(char *URI) {

  if (old_URI==NULL||strcmp(URI,old_URI)) {
    if (old_URI!=NULL) {
      detach_stream(old_URI);
      free(old_URI);
      old_URI=NULL;
    }
    if (!attach_stream(URI)) return NULL;
    old_URI=strdup(URI);
  }

  if (vstream==NULL) return NULL;

  // video part
  cdata.interlace=LIVES_INTERLACE_NONE;

  /* Get format */
  
  cdata.width  = opriv.y_width = tpriv.ti.frame_width;
  cdata.height = opriv.y_height = tpriv.ti.frame_height;
  
  cdata.fps  = (float)tpriv.ti.fps_numerator/(float)tpriv.ti.fps_denominator;
  
  switch(tpriv.ti.pixelformat) {
  case OC_PF_420:
    cdata.palettes[0] = WEED_PALETTE_YUV420P;
    opriv.uv_width = opriv.y_width>>1;
    break;
  case OC_PF_422:
    cdata.palettes[0] = WEED_PALETTE_YUV422P;
    opriv.uv_width = opriv.y_width>>1;
    break;
  case OC_PF_444:
    cdata.palettes[0] = WEED_PALETTE_YUV444P;
    opriv.uv_width = opriv.y_width;
    break;
  default:
    fprintf(stderr, "Unknown pixelformat %d", tpriv.ti.pixelformat);
    return NULL;
  }
  
  cdata.palettes[1]=WEED_PALETTE_END;

  cdata.nframes=vstream->nframes;

  sprintf(cdata.container_name,"%s","ogg");
  sprintf(cdata.video_name,"%s","theora");

  if (astream!=NULL) {
    sprintf(cdata.audio_name,"%s","vorbis");
  }
  else memset(cdata.audio_name,0,1);


  // audio part
  
  return &cdata;
}



static boolean ogg_theora_read(ogg_packet *op, yuv_buffer *yuv) {
  // this packet is for our theora decoder

  if (theora_decode_packetin(&tpriv.ts,op)==0) {
    if (skip<=0) {
      if (theora_decode_YUVout(&tpriv.ts, yuv)==0) {
	frame_out=TRUE;
      }
    }
  }
  return frame_out;
}



static boolean ogg_data_process(yuv_buffer *yuv, boolean cont) {
  int64_t input_pos=0;
  boolean ignore_count=(ignore_packets&&!cont);

  frame_out=FALSE;

  if (!cont) ogg_stream_reset(&vstream->priv->os);

  while (!frame_out) {
    opriv.page_valid=0;

    if (!cont) {
      if (!(input_pos=get_page(input_position))) {
	// should never reach here
	fprintf(stderr, "EOF1 while decoding\n");
	return FALSE;
      }
      
      input_position+=input_pos;
      
      if (ogg_page_serialno(&(opriv.current_page))!=vstream->stream_id) continue;
      ogg_stream_pagein(&vstream->priv->os, &(opriv.current_page));
    }

    while (ogg_stream_packetout(&vstream->priv->os, &opriv.op) > 0) {
      if (yuv!=NULL) {
      // cframe is the frame we are about to decode

	//fprintf(stderr,"cframe is %d\n",cframe);

	if (cframe==last_kframe&&!ignore_count) {
	  ignore_packets=FALSE; // reached kframe before target, start decoding packets
	}
	
	if (!ignore_packets) {
	  ogg_theora_read(&opriv.op,yuv);
	}
      }

      if (!ignore_count) {
	cframe++;
	skip--;
      }
      if (yuv==NULL) frame_out=TRUE;

      if (frame_out) break;
    }

    ignore_count=FALSE;
    cont=FALSE;
  }

  return TRUE;
}


boolean get_frame(char *URI, int64_t tframe, void **pixel_data) {
  // seek to frame, and return pixel_data
  yuv_buffer yuv;
  boolean crow=FALSE;
  void *y,*u,*v;
  int64_t kframe=-1,xkframe;
  boolean cont=FALSE;
  int max_frame_diff=2<<(vstream->priv->keyframe_granule_shift-2);
  int64_t granulepos;
  static int64_t last_cframe;

  int64_t ppos_lower,ppos_upper;

  int64_t current_pos=input_position;

  register int i;

  static index_entry *fidx=NULL;

  int mheight=(opriv.y_height>>1)<<1; // yes indeed, there is a file with a height of 601 pixels...

  if (old_URI==NULL||strcmp(URI,old_URI)) {
    if (old_URI!=NULL) {
      detach_stream(old_URI);
      free(old_URI);
      old_URI=NULL;
    }
    if (!attach_stream(URI)) return FALSE;
    old_URI=strdup(URI);
  }

  tframe+=kframe_offset;

  if (tframe==last_frame) {
    theora_decode_YUVout(&tpriv.ts, &yuv);
  }
  else {
    if (tframe<last_frame||tframe-last_frame>max_frame_diff) {
      // need to find a new kframe
      fidx=get_bounds_for(indexa,tframe,&ppos_lower,&ppos_upper);
      if (fidx==NULL) {
	granulepos=ogg_seek(tframe,ppos_lower,ppos_upper,TRUE);
	if (granulepos==-1) return FALSE; // should never happen...
	indexa=index_entry_add(indexa,granulepos,cpagepos,&fidx);
      }
      else granulepos=fidx->granulepos;
      kframe = granulepos >> vstream->priv->keyframe_granule_shift;
      if (kframe<kframe_offset) kframe=kframe_offset;
    }
    else kframe=last_kframe;
    
    ignore_packets=FALSE;
    
    if (tframe>last_frame&&((tframe-last_frame<=max_frame_diff||kframe==last_kframe))) {
      // same keyframe as last time, or next frame; we can continue where we left off
      cont=TRUE;
      skip=tframe-last_frame-1;
      input_position=current_pos;
    }
    else {
      if (fidx==NULL||fidx->prev==NULL) {
	get_bounds_for(indexa,kframe-1,&ppos_lower,&ppos_upper);
	granulepos=ogg_seek(kframe-1,ppos_lower,ppos_upper,FALSE);
	//fprintf(stderr,"starting from found gpos %lld\n",granulepos);
	xkframe=granulepos >> vstream->priv->keyframe_granule_shift;
	cframe = xkframe + granulepos-(xkframe<<vstream->priv->keyframe_granule_shift)+1; // cframe will be the next frame we decode
	if (input_position==data_start) {
	  cframe=kframe=1;
	}
	else {
	  indexa=index_entry_add(indexa,granulepos,input_position,NULL);
	}
	last_cframe=cframe;
	skip=tframe-cframe;
      }
      else {
	// same keyframe as last time, but we are reversing
	input_position=fidx->prev->pagepos;
	granulepos=fidx->prev->granulepos;
	//fprintf(stderr,"starting from gpos %lld\n",granulepos);
	xkframe=granulepos >> vstream->priv->keyframe_granule_shift;
	cframe = xkframe + granulepos-(xkframe<<vstream->priv->keyframe_granule_shift)+1; // cframe will be the next frame we decode
	skip=tframe-cframe;
      }
      
      if (input_position>data_start) {
	ignore_packets=TRUE;
      }
      
    }
    
    //fprintf(stderr,"getting yuv at %lld\n",input_position);
    
    last_frame=tframe;
    last_kframe=kframe;
    if (!ogg_data_process(&yuv,cont)) return FALSE;
  }

  y=yuv.y;
  u=yuv.u;
  v=yuv.v;

  for (i=0;i<mheight;i++) {
    memcpy(pixel_data[0],y,opriv.y_width);
    pixel_data[0]+=opriv.y_width;
    y+=yuv.y_stride;
    if (yuv.y_height==yuv.uv_height||crow) {
      memcpy(pixel_data[1],u,opriv.uv_width);
      memcpy(pixel_data[2],v,opriv.uv_width);
      pixel_data[1]+=opriv.uv_width;
      pixel_data[2]+=opriv.uv_width;
      u+=yuv.uv_stride;
      v+=yuv.uv_stride;
    }
    crow=!crow;
  }

  return TRUE;

}
  


void module_unload(void) {
  if (old_URI!=NULL) {
    detach_stream(old_URI);
    free(old_URI);
  }
  free(cdata.palettes);
}

