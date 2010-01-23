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
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <libdv/dv.h>

#include "dv_decoder.h"

static lives_clip_data_t cdata;
static dv_priv_t priv;

static char *old_URI=NULL;

const char *plugin_version="LiVES dv decoder version 1.0";

static int mypalette=WEED_PALETTE_END;

static FILE *nulfile;


static void _set_palette(int palette) {
  switch (palette) {
  case WEED_PALETTE_YUYV8888:
    cdata.width=360;
    cdata.YUV_clamping=WEED_YUV_CLAMPING_UNCLAMPED;
    cdata.YUV_subspace=WEED_YUV_SUBSPACE_YCBCR;
    break;
  case WEED_PALETTE_RGB24:
    cdata.width=720;
    break;
  case WEED_PALETTE_BGR24:
    cdata.width=720;
    break;
  }
  mypalette=palette;
}



static void dv_dec_set_header(uint8_t *data) {
  if((data[3] & 0x80) == 0) {      /* DSF flag */
    // NTSC

    priv.frame_size = 120000;
    priv.is_pal = 0;

    cdata.height=480;

    cdata.fps=30000./1001.;

    
  }
  else {
    // PAL

    priv.frame_size = 144000;
    priv.is_pal = 1;

    cdata.height=576;

    cdata.fps=25.;

  }
}


static boolean attach_stream(char *URI) {
  // open the file and get a handle
  struct stat sb;
  uint8_t header[DV_HEADER_SIZE];
  uint8_t *fbuffer;

  char *ext=rindex(URI,'.');

  if (ext==NULL||strncmp(ext,".dv",3)) return FALSE;

  if (!(priv.fd=open(URI,O_RDONLY))) {
    fprintf(stderr, "dv_decoder: unable to open %s\n",URI);
    return FALSE;
  }

  if (read (priv.fd, header, DV_HEADER_SIZE) < DV_HEADER_SIZE) {
    fprintf(stderr, "dv_decoder: unable to read header for %s\n",URI);
    return FALSE;
  }

  priv.dv_dec=dv_decoder_new(0,0,0); // ignored, unclamp_luma, unclamp_chroma

  dv_set_error_log(priv.dv_dec,nulfile);

  dv_dec_set_header(header);
  dv_parse_header(priv.dv_dec,header);

  lseek(priv.fd,0,SEEK_SET);
  fbuffer=malloc(priv.frame_size);
  if (read (priv.fd, fbuffer, priv.frame_size) < priv.frame_size) {
    fprintf(stderr, "dv_decoder: unable to read first frame for %s\n",URI);
    return FALSE;
  }
  dv_parse_audio_header(priv.dv_dec,fbuffer);
  free(fbuffer);

  fstat(priv.fd,&sb);

  if (sb.st_size) cdata.nframes = (int)(sb.st_size / priv.frame_size);
  
  priv.dv_dec->quality=DV_QUALITY_BEST;
  //priv.dv_dec->add_ntsc_setup=TRUE;
    
  return TRUE;
}

static void detach_stream (char *URI) {
  // close the file, free the decoder
  close(priv.fd);
  dv_decoder_free(priv.dv_dec);
}


//////////////////////////////////////////
// std functions



const char *module_check_init(void) {
  cdata.palettes=malloc(4*sizeof(int));

  // plugin allows a choice of palettes; we set these in order of preference
  // and implement a set_palette() function
  cdata.palettes[0]=WEED_PALETTE_YUYV8888;
  cdata.palettes[1]=WEED_PALETTE_RGB24;
  cdata.palettes[2]=WEED_PALETTE_BGR24;
  cdata.palettes[3]=WEED_PALETTE_END;

  nulfile=fopen("/dev/null","a");

  return NULL;
}


const char *version(void) {
  return plugin_version;
}



boolean set_palette(int palette) {
  if (palette==WEED_PALETTE_YUYV8888||palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) {
    _set_palette(palette);
    return TRUE;
  }
  return FALSE;
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

  sprintf(cdata.container_name,"%s","dv");

  memset(cdata.video_name,0,1);
  memset(cdata.audio_name,0,1);

  // video part
  cdata.interlace=LIVES_INTERLACE_BOTTOM_FIRST;

  // audio part
  cdata.arate=dv_get_frequency(priv.dv_dec);
  cdata.achans=dv_get_num_channels(priv.dv_dec);
  cdata.asamps=16;

  cdata.asigned=0;
  cdata.ainterleaf=0;

  return &cdata;
}



static boolean dv_pad_with_silence(int fd, size_t zbytes) {
  unsigned char *silencebuf=calloc(zbytes,1);

  if (write(fd,silencebuf, zbytes) != zbytes) {
    free(silencebuf);
    return FALSE;
  }

  free(silencebuf);
  return TRUE;
}



boolean rip_audio (char *URI, char *fname, int stframe, int frames) {
  // rip audio from stframe length frames from URI
  // to file fname

  // stframe starts at 0

  // if frames==0, rip all audio

  // output seems to be always 16bit per sample

  // sometimes we get fewer samples than expected, so we do two passes and set
  // scale on the first pass. This is then used to resample on the second pass

  int16_t *audio_buffers[4],*audio;
  int i,j,ch,channels,samples;
  size_t bytes;
  off64_t stbytes=stframe*priv.frame_size;
  uint8_t buf[priv.frame_size];
  int out_fd;
  int xframes=frames;
  double scale=0.;
  double offset_f=0.;

  long samps_expected=(double)(frames==0?cdata.nframes:frames)/cdata.fps*cdata.arate, samps_actual=0;

  off_t offset_i=0;

  for(i=0;i<4;i++) {
    if (!(audio_buffers[i] = (int16_t *)malloc(DV_AUDIO_MAX_SAMPLES * 2 * sizeof(int16_t)))) {
      fprintf(stderr, "dv_decoder: out of memory\n");
      return FALSE;
    }
  }

  if(!(audio = malloc(DV_AUDIO_MAX_SAMPLES * 8 * sizeof(int16_t)))) {
    fprintf(stderr, "dv_decoder: out of memory\n");
    return FALSE;
  }

  if (!(out_fd=open(fname,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR))) {
    fprintf(stderr, "dv_decoder: unable to open output %s\n",fname);
    return FALSE;
  }

  if (old_URI==NULL||strcmp(URI,old_URI)) {
    if (old_URI!=NULL) {
      detach_stream(old_URI);
      free(old_URI);
      old_URI=NULL;
    }
    if (!attach_stream(URI)) return FALSE;
    old_URI=strdup(URI);
  }

  channels = priv.dv_dec->audio->num_channels;

  lseek64(priv.fd,stbytes,SEEK_SET);

  while (1) {
    if (read (priv.fd, buf, priv.frame_size) < priv.frame_size) break;
    dv_parse_header(priv.dv_dec, buf);

    samples  = priv.dv_dec->audio->samples_this_frame;
    samps_actual += samples;

    if (frames>0) if (--frames==0) break;
  }


  scale=(long double)samps_actual/(long double)samps_expected-1.;

  frames=xframes;

  lseek64(priv.fd,stbytes,SEEK_SET);

  while (1) {
    if (read (priv.fd, buf, priv.frame_size) < priv.frame_size) break;
    dv_parse_header(priv.dv_dec, buf);
    
    samples  = priv.dv_dec->audio->samples_this_frame;
    
    dv_decode_full_audio(priv.dv_dec, buf, audio_buffers);

    // interleave the audio into a single buffer
    j=0;
    for(i=0;i<samples;i++) {
      offset_i=(size_t)offset_f;
      
      for(ch=0;ch<channels;ch++) {
	audio[j++] = audio_buffers[ch][i+offset_i];
      }
      
      offset_f+=scale;

      if (offset_f<-1.&&i>0) {
	// slipped back a whole sample
	offset_f+=1.;
	i--;
	samples++;
      }
      
      
      if (offset_f>1.) {
	// slipped forward a whole sample
	offset_f-=1.;
	i++;
	samples--;
      }
    }
      
    samps_expected-=samples;
    
    if (samps_expected<0) samples+=samps_expected;
    
    bytes = samples*channels*2;
    
    // write out
    if (write(out_fd, (char*) audio, bytes) != bytes) {
      fprintf(stderr, "dv_decoder: audio write error %s\n",fname);
      close(out_fd);
      return FALSE;
    }
    
    if (frames>0) if (--frames==0) break;
  }


  for(i=0;i<4;i++) {
    free(audio_buffers[i]);
  }
  free(audio);

  // pad to end with silence
  if (samps_expected) if (!dv_pad_with_silence(out_fd,samps_expected*channels*2)) {
    fprintf(stderr, "dv_decoder: audio write error %s\n",fname);
    close(out_fd);
    return FALSE;
  }

  close(out_fd);

  return TRUE;

}




boolean get_frame(char *URI, int64_t tframe, void **pixel_data) {
  // seek to frame, and return width, height and pixel_data

  // tframe starts at 0

  uint8_t fbuffer[priv.frame_size];
  
  int rowstrides[1];

  int64_t frame=tframe;
  off64_t bytes=frame*priv.frame_size;
  

  if (mypalette==WEED_PALETTE_END) {
    fprintf(stderr,"Host must set palette using set_palette(int palette)\n");
    return FALSE;
  }

  if (old_URI==NULL||strcmp(URI,old_URI)) {
    if (old_URI!=NULL) {
      detach_stream(old_URI);
      free(old_URI);
      old_URI=NULL;
    }
    if (!attach_stream(URI)) return FALSE;
    old_URI=strdup(URI);
  }

  lseek64(priv.fd,bytes,SEEK_SET);
    
  if (read (priv.fd, fbuffer, priv.frame_size) < priv.frame_size) return FALSE;
  
  dv_parse_header(priv.dv_dec, fbuffer);
  dv_set_error_log(priv.dv_dec,nulfile);
  
  switch (mypalette) {
  case WEED_PALETTE_RGB24:
    rowstrides[0]=cdata.width*3;
    dv_decode_full_frame(priv.dv_dec,fbuffer,e_dv_color_rgb,(uint8_t **)pixel_data,rowstrides);
    break;
  case WEED_PALETTE_BGR24:
    rowstrides[0]=cdata.width*3;
    dv_decode_full_frame(priv.dv_dec,fbuffer,e_dv_color_bgr0,(uint8_t **)pixel_data,rowstrides);
    break;
  case WEED_PALETTE_YUYV8888:
    rowstrides[0]=cdata.width*4; // 4 bytes per macropixel
    dv_decode_full_frame(priv.dv_dec,fbuffer,e_dv_color_yuv,(uint8_t **)pixel_data,rowstrides);
    break;
  }

  return TRUE;
}



void module_unload(void) {
  if (old_URI!=NULL) {
    detach_stream(old_URI);
    free(old_URI);
  }
  free(cdata.palettes);

  fclose(nulfile);
}
