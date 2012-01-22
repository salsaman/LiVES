// farneback_analyser.cpp
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// Computes a dense optical flow using the Gunnar Farneback’s algorithm.

/*
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
following conditions are met:

    Redistributions of source code must retain the above copyright notice, this list of conditions 
    and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice, this list of conditions 
    and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// calculates array flow, s.t.:
// prevImg(y,x) = nextImg( y + flow(y,x)[0], x + flow(y,x)[1] )

// input is nextImg : this is cached in memory to provide prevImg for the next call
// output is two channels of type ALPHA FLOAT (the flow(y,x)[0] in the first, and the flow(y,x)[1] in the second)


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]={131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-utils.h> // optional
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

#define FP_BITS 16

int myround(double n) {
  return (n>=0.)?(int)(n + 0.5):(int)(n - 0.5);
}

#include "opencv2/core/core.hpp"
#include "opencv2/video/tracking.hpp"
using namespace cv;


typedef struct {
  uint8_t *prevgrey;
} _sdata;



static uint8_t UNCLAMP_Y[256];

// unclamped Y'CbCr
static uint32_t Y_Ru[256];
static uint32_t Y_Gu[256];
static uint32_t Y_Bu[256];


static void init_luma_arrays(void) {
  register int i;

  for (i = 0; i < 256; i++) {
    Y_Ru[i] = myround(0.299 * (double)i
		      * (1<<FP_BITS));
    Y_Gu[i] = myround(0.587 * (double)i 
		      * (1<<FP_BITS));
    Y_Bu[i] = myround(0.114 * (double)i 
                      * (1<<FP_BITS));

  }

  for (i=0;i<17;i++) {
    UNCLAMP_Y[i]=0;
  }

  for (i=17;i<235;i++) {
    UNCLAMP_Y[i]=(int)((float)(i-16.)/219.*255.+.5);
  }

  for (i=235;i<256;i++) {
    UNCLAMP_Y[i]=255;
  }

}


static uint8_t rgb_to_y(int r, int g, int b) {
  short x;
  if ((x=((Y_Ru[r]+Y_Gu[g]+Y_Bu[b])>>FP_BITS))>255) x=255;
  return x<0?0:(uint8_t)x;
}
     

static void unclamp_frame(uint8_t *data, int width, int height) {
  register int i,j;
  for (i=0;i<height;i++) {
    for (j=0;j<width;j++) {
      *data++=UNCLAMP_Y[*data];
    }
  }
}


static uint8_t *rgb_to_y_frame(const uint8_t *csrc, int width, int row, int height, off_t r, off_t g, off_t b, off_t a) {
  register int i,j;
  int offs=0;
  int psize=4;
  int widthx;

  uint8_t *src=(uint8_t *)csrc;
  uint8_t *dst;

  if (a==-1) psize=3;
  if (a==0) offs=1;

  widthx=width*psize;
  row-=widthx;

  dst=(uint8_t *)weed_malloc(width*height);

  if (!dst) return NULL;

  for (i=0;i<height;i++) {
    for (j=offs;j<widthx;j+=psize) {
      *dst++=rgb_to_y(*(src+r), *(src+g), *(src+b));
      src+=psize;
    }
    src+=row;
  }
  return dst;
}



static uint8_t *copy_frame(const uint8_t *csrc, int width, int row, int height) {
  uint8_t *src=(uint8_t *)csrc;
  uint8_t *dst=(uint8_t *)weed_malloc(width*height);
  if (!dst) return NULL;
  if (width==row) weed_memcpy(dst,src,width*height);
  else {
    uint8_t *dstp=dst;
    register int i;
    for (i=0;i<height;i++) {
      weed_memcpy(dstp,src,width);
      dstp+=width;
      src+=row;
    }
  }
  return dst;
}




int farneback_init (weed_plant_t *inst) {
  int error;
  _sdata *sdata;

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->prevgrey=NULL;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  return WEED_NO_ERROR;
}



int farneback_deinit (weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  if (sdata!=NULL) {
    if (sdata->prevgrey!=NULL) weed_free(sdata->prevgrey);
    weed_free(sdata);
  }
  return WEED_NO_ERROR;
}




int farneback_process (weed_plant_t *inst, weed_timecode_t tc) {
  int error;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t **out_channels=weed_get_plantptr_array(inst,"out_channels",&error);

  uint8_t *src=(uint8_t *)weed_get_voidptr_value(in_channel,"pixel_data",&error);

  uint8_t *dst1=(uint8_t *)weed_get_voidptr_value(out_channels[0],"pixel_data",&error);
  uint8_t *dst2=(uint8_t *)weed_get_voidptr_value(out_channels[1],"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int palette=weed_get_int_value(in_channel,"current_palette",&error);

  int irow=weed_get_int_value(in_channel,"rowstrides",&error);
  int orow1=weed_get_int_value(out_channels[0],"rowstrides",&error);
  int orow2=weed_get_int_value(out_channels[1],"rowstrides",&error);
  
  int mustcopy=0;
  int clamped=0;

  uint8_t *grey;

  Mat cvgrey, cvprevgrey, cvflow;

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  // convert image to greyscale

  switch (palette) {
  case WEED_PALETTE_RGB24:
    grey=rgb_to_y_frame(src,width,irow,height,0, 1, 2, -1);
    break;
  case WEED_PALETTE_BGR24:
    grey=rgb_to_y_frame(src,width,irow,height,2, 1, 0, -1);
    break;
  case WEED_PALETTE_RGBA32:
    grey=rgb_to_y_frame(src,width,irow,height,0, 1, 2, 3);
    break;
  case WEED_PALETTE_BGRA32:
    grey=rgb_to_y_frame(src,width,irow,height,2, 1, 0, 3);
    break;
  case WEED_PALETTE_ARGB32:
    grey=rgb_to_y_frame(src,width,irow,height,1, 2, 3, 0);
    break;
  case WEED_PALETTE_YUVA4444P:
  case WEED_PALETTE_YUV444P:
  case WEED_PALETTE_YUV422P:
  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    grey=copy_frame(src,width,irow,height);
    if (weed_plant_has_leaf(in_channel,"YUV_clamping")&&
	(weed_get_int_value(in_channel,"YUV_clamping",&error)==WEED_YUV_CLAMPING_CLAMPED))
      unclamp_frame(grey,width,height);
    break;
  default:
    break;
  }

  if (!grey) return WEED_ERROR_MEMORY_ALLOCATION;

  if (sdata->prevgrey==NULL) {
    sdata->prevgrey=grey;
    return WEED_NO_ERROR;
  }


  cvgrey=Mat(width,height,CV_8U,grey);
  cvprevgrey=Mat(width,height,CV_8U,sdata->prevgrey);


  /*
    Parameters:	

        prevImg – First 8-bit single-channel input image.
        nextImg – Second input image of the same size and the same type as prevImg .
        flow – Computed flow image that has the same size as prevImg and type CV_32FC2 .
        pyrScale – Parameter specifying the image scale (<1) to build pyramids for each image. pyrScale=0.5 means a classical pyramid, where each next layer is twice smaller than the previous one.
        levels – Number of pyramid layers including the initial image. levels=1 means that no extra layers are created and only the original images are used.
        winsize – Averaging window size. Larger values increase the algorithm robustness to image noise and give more chances for fast motion detection, but yield more blurred motion field.
        iterations – Number of iterations the algorithm does at each pyramid level.
        polyN – Size of the pixel neighborhood used to find polynomial expansion in each pixel. Larger values mean that the image will be approximated with smoother surfaces, yielding more robust algorithm and more blurred motion field. Typically, polyN =5 or 7.
        polySigma – Standard deviation of the Gaussian that is used to smooth derivatives used as a basis for the polynomial expansion. For polyN=5 , you can set polySigma=1.1 . For polyN=7 , a good value would be polySigma=1.5 .
        flags –

        Operation flags that can be a combination of the following:
            OPTFLOW_USE_INITIAL_FLOW Use the input flow as an initial flow approximation.
            OPTFLOW_FARNEBACK_GAUSSIAN Use the Gaussian \texttt{winsize}\times\texttt{winsize} filter instead of a box filter of the same size for optical flow estimation. Usually, this option gives z more accurate flow than with a box filter, at the cost of lower speed. Normally, winsize for a Gaussian window should be set to a larger value to achieve the same level of robustness.

The function finds an optical flow for each prevImg pixel using the [Farneback2003] alorithm so that

prevImg (y,x) = nextImg ( y + flow (y,x)[1], x + flow (y,x)[0])


  */

  fprintf(stderr,"analysing\n");
  calcOpticalFlowFarneback(cvprevgrey, cvgrey, cvflow, 0.5, 3, 15, 3, 5, 1.2, 0);
  fprintf(stderr,"analysing done\n");
  

  weed_free(sdata->prevgrey);
  sdata->prevgrey=grey;

  // copy cvflow to float outputs


  // clean up

  weed_free(out_channels);
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int ipalette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,
			 WEED_PALETTE_YUVA4444P,
			 WEED_PALETTE_YUV444P,
			 WEED_PALETTE_YUV422P,
			 WEED_PALETTE_YUV420P,
			 WEED_PALETTE_YVU420P,
			 WEED_PALETTE_END};
    // define a vector output
    int opalette_list[]={WEED_PALETTE_AFLOAT,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel",
							     WEED_CHANNEL_REINIT_ON_SIZE_CHANGE|
							     WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE|
							     WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE,
							     ipalette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("X values",0,opalette_list),
				   weed_channel_template_init("Y values",0,opalette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("farneback_analyser","salsaman",1,0,&farneback_init,
						      &farneback_process,&farneback_deinit,
						      in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

    init_luma_arrays();
  }
  return plugin_info;
}

