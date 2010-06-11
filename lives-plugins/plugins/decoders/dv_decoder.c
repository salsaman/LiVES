// LiVES - dv decoder plugin
// (c) G. Finch 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// NOTE: interlace is bottom first

#include "decplugin.h"

// palettes, etc.
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-effects.h"


///////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <libdv/dv.h>

#include "dv_decoder.h"

const char *plugin_version="LiVES dv decoder version 1.2";

static FILE *nulfile;

static void dv_dec_set_header(lives_clip_data_t *cdata, uint8_t *data) {
  lives_dv_priv_t *priv=cdata->priv;

  if((data[3] & 0x80) == 0) {      /* DSF flag */
    // NTSC

    priv->frame_size = 120000;
    priv->is_pal = 0;
    cdata->height=480;
    cdata->fps=30000./1001.;
    
  }
  else {
    // PAL

    priv->frame_size = 144000;
    priv->is_pal = 1;
    cdata->height=576;
    cdata->fps=25.;
  }
}


static boolean attach_stream(lives_clip_data_t *cdata) {
  // open the file and get a handle
  struct stat sb;
  uint8_t header[DV_HEADER_SIZE];
  uint8_t *fbuffer;
  lives_dv_priv_t *priv=cdata->priv;

  char *ext=rindex(cdata->URI,'.');

  if (ext==NULL||strncmp(ext,".dv",3)) return FALSE;

  if ((priv->fd=open(cdata->URI,O_RDONLY))==-1) {
    fprintf(stderr, "dv_decoder: unable to open %s\n",cdata->URI);
    return FALSE;
  }

  if (read (priv->fd, header, DV_HEADER_SIZE) < DV_HEADER_SIZE) {
    fprintf(stderr, "dv_decoder: unable to read header for %s\n",cdata->URI);
    close(priv->fd);
    return FALSE;
  }

  priv->dv_dec=dv_decoder_new(0,0,0); // ignored, unclamp_luma, unclamp_chroma

  dv_set_error_log(priv->dv_dec,nulfile);

  dv_dec_set_header(cdata, header);
  dv_parse_header(priv->dv_dec,header);

  lseek64(priv->fd,0,SEEK_SET);
  fbuffer=malloc(priv->frame_size);
  if (read (priv->fd, fbuffer, priv->frame_size) < priv->frame_size) {
    fprintf(stderr, "dv_decoder: unable to read first frame for %s\n",cdata->URI);
    free(fbuffer);
    close(priv->fd);
    dv_decoder_free(priv->dv_dec);
    return FALSE;
  }
  dv_parse_audio_header(priv->dv_dec,fbuffer);
  free(fbuffer);

  fstat(priv->fd,&sb);

  if (sb.st_size) cdata->nframes = (int)(sb.st_size / priv->frame_size);
  
  priv->dv_dec->quality=DV_QUALITY_BEST;
  //priv.dv_dec->add_ntsc_setup=TRUE;
    
  return TRUE;
}



static void detach_stream (lives_clip_data_t *cdata) {
  // close the file, free the decoder
  lives_dv_priv_t *priv=cdata->priv;
  close(priv->fd);
  dv_decoder_free(priv->dv_dec);
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {

  nulfile=fopen("/dev/null","a");

  return NULL;
}


const char *version(void) {
  return plugin_version;
}



static lives_clip_data_t *init_cdata (void) {
  register int i;
  lives_dv_priv_t *priv;
  lives_clip_data_t *cdata=(lives_clip_data_t *)malloc(sizeof(lives_clip_data_t));
  
  cdata->palettes=malloc(4*sizeof(int));
  
  // plugin allows a choice of palettes; we set these in order of preference
  // and implement a set_palette() function
  cdata->palettes[0]=WEED_PALETTE_YUYV8888;
  cdata->palettes[1]=WEED_PALETTE_RGB24;
  cdata->palettes[2]=WEED_PALETTE_BGR24;
  cdata->palettes[3]=WEED_PALETTE_END;
  
  cdata->URI=NULL;
  
  cdata->priv=priv=malloc(sizeof(lives_dv_priv_t));

  for (i=0;i<4;i++) {
    priv->audio_buffers[i]=NULL;
  }
  
  priv->audio=NULL;
  priv->audio_fd=-1;
  
  return cdata;
}






lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *cdata) {
  // the first time this is called, caller should pass NULL as the cdata
  // subsequent calls to this should re-use the same cdata

  // if the host wants a different URI or a different current_clip, this must be called again with the same
  // cdata as the second parameter

  lives_dv_priv_t *priv;

  if (cdata!=NULL&&cdata->current_clip>0) {
    // currently we only support one clip per container

    clip_data_free(cdata);
    return NULL;
  }

  if (cdata==NULL) {
    cdata=init_cdata();
  }

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
    cdata->current_palette=cdata->palettes[0];
    cdata->current_clip=0;
  }

  cdata->nclips=1;

  sprintf(cdata->container_name,"%s","dv");

  sprintf(cdata->video_name,"%s","dv");

  sprintf(cdata->audio_name,"%s","pcm");

  // video part
  switch (cdata->current_palette) {
  case WEED_PALETTE_YUYV8888:
    cdata->width=360;
    cdata->YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
    cdata->YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
    cdata->YUV_sampling=WEED_YUV_SAMPLING_DEFAULT; // TODO - may be different for PAL/NTSC
    break;
  case WEED_PALETTE_RGB24:
    cdata->width=720;
    break;
  case WEED_PALETTE_BGR24:
    cdata->width=720;
    break;
  default:
    fprintf(stderr,"Error - invalid palette set in dv decoder !\n");
  }

  // cdata->height was set when we attached the stream

  cdata->interlace=LIVES_INTERLACE_BOTTOM_FIRST;

  cdata->par=1.;
  cdata->offs_x=0;
  cdata->offs_y=0;
  cdata->frame_width=cdata->width;
  cdata->frame_height=cdata->height;

  // audio part
  priv=cdata->priv;
  cdata->arate=dv_get_frequency(priv->dv_dec);
  cdata->achans=dv_get_num_channels(priv->dv_dec);
  cdata->asamps=16;

  cdata->asigned=TRUE;
  cdata->ainterleaf=FALSE;

  return cdata;
}



static boolean dv_pad_with_silence(int fd, unsigned char **abuff, size_t offs, int nchans, size_t nsamps) {
  unsigned char *silencebuf;
  size_t xoffs=offs-2;
  register int i,j;

  nsamps*=2; // 16 bit samples

  if (fd!=-1) {
    // write to file
    size_t zbytes=nsamps*nchans;
    silencebuf=calloc(nsamps,nchans);

    if (write(fd,silencebuf,zbytes) != zbytes) {
      free(silencebuf);
      return FALSE;
    }
    
    free(silencebuf);
  }

  if (abuff!=NULL) {
    // write to memory
    for (j=0;j<nchans;j++) {
      // write 0
      if (xoffs<0) memset(&(abuff[j][offs]),0,nsamps);
      else {
	// or copy last bytes
	for (i=0;i<nsamps;i+=2) {
	  memcpy(&(abuff[j][offs+i]),&(abuff[j][xoffs]),2);
	}
      }
    }
  }

  return TRUE;
}


void rip_audio_cleanup(const lives_clip_data_t *cdata) {
  register int i;
  lives_dv_priv_t *priv=cdata->priv;

  for (i=0;i<4;i++) {
    if (priv->audio_buffers[i]!=NULL) free(priv->audio_buffers[i]);
    priv->audio_buffers[i]=NULL;
  }

  if (priv->audio!=NULL) free(priv->audio);
  priv->audio=NULL;

  if (priv->audio_fd!=-1) close (priv->audio_fd);

}



int64_t rip_audio (const lives_clip_data_t *cdata, const char *fname, int64_t stframe, int64_t nframes, 
		   unsigned char **abuff) {
  // rip audio from (video) frame stframe, length nframes (video) frames from cdata
  // to file fname

   // if nframes==0, rip all audio



  // (output seems to be always 16bit per sample

  // sometimes we get fewer samples than expected, so we do two passes and set
  // scale on the first pass. This is then used to resample on the second pass)

  // note: host can kill this function with SIGUSR1 at any point after we start writing the 
  //       output file

  // note: host will call rip_audio_cleanup() after calling here

  // return number of samples written

  // if fname is NULL we write to abuff instead (unless abuff is NULL)


  int i,ch,channels,samples,samps_out;
  size_t j=0,k=0,bytes;
  off64_t stbytes;
  uint8_t *buf;
  int xframes;
  double scale=0.;
  double offset_f=0.;

  int64_t samps_actual=0,samps_expected,tot_samples=0;

  lives_dv_priv_t *priv=cdata->priv;

  if (fname==NULL&&abuff==NULL) return 0;

  if (nframes==0) nframes=cdata->nframes;
  if (nframes>cdata->nframes) nframes=cdata->nframes;

  xframes=nframes;

  for (i=0;i<4;i++) {
    if (priv->audio_buffers[i]==NULL) {
      if (!(priv->audio_buffers[i] = (int16_t *)malloc(DV_AUDIO_MAX_SAMPLES * 2 * sizeof(int16_t)))) {
	fprintf(stderr, "dv_decoder: out of memory\n");
	return 0;
      }
    }
  }

  if (priv->audio==NULL) {
    if(!(priv->audio = malloc(DV_AUDIO_MAX_SAMPLES * 8 * sizeof(int16_t)))) {
      for (i=0;i<4;i++) {
	free(priv->audio_buffers[i]);
	priv->audio_buffers[i]=NULL;
      }
      fprintf(stderr, "dv_decoder: out of memory\n");
      return 0;
    }
  }

  samps_expected=(double)(nframes)/cdata->fps*cdata->arate;

  // do this last so host knows we are ready
  if (fname!=NULL) {
    if ((priv->audio_fd=open(fname,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR))==-1) {
      fprintf(stderr, "dv_decoder: unable to open output %s\n",fname);
      return 0;
    }
  }

  /////////////////////////////////////////////////////////////////////////


  stbytes=stframe*priv->frame_size;

  channels = priv->dv_dec->audio->num_channels;

  lseek64(priv->fd,stbytes,SEEK_SET);

  buf=malloc(priv->frame_size);

  while (1) {
    // decode frame headers and total number of samples

    if (read (priv->fd, buf, priv->frame_size) < priv->frame_size) break;
    dv_parse_header(priv->dv_dec, buf);

    samples  = priv->dv_dec->audio->samples_this_frame;
    samps_actual += samples;

    if (--nframes==0) break;
  }

  if (samps_actual==samps_expected+1) samps_expected++;


  // we may get more or fewer samples than expected, so we need to scale
  scale=(long double)samps_actual/(long double)samps_expected-1.;

  nframes=xframes;

  lseek64(priv->fd,stbytes,SEEK_SET);

  while (1) {
    // now we do the actual decoding, outputting to file and/or memory

    samps_out=0;

    if (read (priv->fd, buf, priv->frame_size) < priv->frame_size) break;
    dv_parse_header(priv->dv_dec, buf);
    
    samples  = priv->dv_dec->audio->samples_this_frame;
    
    dv_decode_full_audio(priv->dv_dec, buf, priv->audio_buffers);

    j=0;

    // interleave the audio into a single buffer
    for (i=0;i<samples&&samps_expected;i++) {
      
      for (ch=0;ch<channels;ch++) {
	if (fname!=NULL) priv->audio[j++] = priv->audio_buffers[ch][i];
	else {
	  // copy a 16 bit sample
	  memcpy(&(abuff[ch][k]),&(priv->audio_buffers[ch][i]),2);
	}
      }

      k+=2;
      
      offset_f+=scale;

      if (offset_f<=-1.&&i>0) {
	// slipped back a whole sample, process same i
	offset_f+=1.;
	i--;
      }
      
      
      if (offset_f>=1.) {
	// slipped forward a whole sample, process i+2
	offset_f-=1.;
	i++;
      }

      samps_expected--;
      samps_out++;
    }
    
    bytes = samps_out*channels*2;
    
    tot_samples+=samps_out;

    // write out
    if (fname!=NULL) {
      if (write(priv->audio_fd, (char*) priv->audio, bytes) != bytes) {
	free(buf);
	fprintf(stderr, "dv_decoder: audio write error %s\n",fname);
	return tot_samples;
      }
    }
    
    if (--nframes==0) break;
  }

  free(buf);


  // not enough samples - pad to end with silence
  if (samps_expected&&fname!=NULL) {
    if (!dv_pad_with_silence(fname!=NULL?priv->audio_fd:-1,abuff,j,channels,samps_expected)) {
      fprintf(stderr, "dv_decoder: audio write error %s\n",fname!=NULL?fname:"to memory");
    }
    tot_samples+=samps_expected;
  }

  return tot_samples;
}




boolean get_frame(const lives_clip_data_t *cdata, int64_t tframe, void **pixel_data) {
  // seek to frame, and return width, height and pixel_data

  // tframe starts at 0

  lives_dv_priv_t *priv=cdata->priv;

  uint8_t fbuffer[priv->frame_size];
  
  int rowstrides[1];

  int64_t frame=tframe;
  off64_t bytes=frame*priv->frame_size;
  
  lseek64(priv->fd,bytes,SEEK_SET);
    
  if (read (priv->fd, fbuffer, priv->frame_size) < priv->frame_size) return FALSE;
  
  dv_parse_header(priv->dv_dec, fbuffer);
  dv_set_error_log(priv->dv_dec,nulfile);
  
  switch (cdata->current_palette) {
  case WEED_PALETTE_RGB24:
    rowstrides[0]=cdata->width*3;
    dv_decode_full_frame(priv->dv_dec,fbuffer,e_dv_color_rgb,(uint8_t **)pixel_data,rowstrides);
    break;
  case WEED_PALETTE_BGR24:
    rowstrides[0]=cdata->width*3;
    dv_decode_full_frame(priv->dv_dec,fbuffer,e_dv_color_bgr0,(uint8_t **)pixel_data,rowstrides);
    break;
  case WEED_PALETTE_YUYV8888:
    rowstrides[0]=cdata->width*4; // 4 bytes per macropixel
    dv_decode_full_frame(priv->dv_dec,fbuffer,e_dv_color_yuv,(uint8_t **)pixel_data,rowstrides);
    break;
  default:
    fprintf(stderr,"Error - invalid palette in dv decoder !\n");
    return FALSE;
  }

  return TRUE;
}




void clip_data_free(lives_clip_data_t *cdata) {

  if (cdata->URI!=NULL) {
    detach_stream(cdata);
    free(cdata->URI);
  }

  free(cdata->priv);

  free(cdata->palettes);
  free(cdata);
}


void module_unload(void) {
  fclose(nulfile);
}
